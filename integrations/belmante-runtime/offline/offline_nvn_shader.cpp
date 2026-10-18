// SPDX-License-Identifier: GPL-3.0-or-later
// Offline diagnostic only: explicit raster profile; no GPU or guest execution.
#include "native_shaders.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/maxwell/translate_program.h"
#include "shader_recompiler/host_translate_info.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <stdexcept>
#include <map>
#include <charconv>
std::vector<std::byte> read(const char* name, size_t limit) {
    std::ifstream f(name,std::ios::binary|std::ios::ate);
    const auto size=f.tellg();
    if(!f||size<0||static_cast<uint64_t>(size)>limit)throw std::runtime_error("input size/read failure");
    std::vector<std::byte> b(static_cast<size_t>(size));f.seekg(0);
    if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("truncated input");
    return b;
}
uint32_t word(const std::vector<std::byte>& b,size_t offset) {
    if(offset>b.size()||b.size()-offset<4)throw std::runtime_error("control read out of bounds");
    uint32_t v;std::memcpy(&v,b.data()+offset,4);return v;
}
int main(int argc,char** argv) {try {
    if(argc!=5)throw std::runtime_error("Usage: offline_nvn_shader code.bin ctrl.bin viewport-transform-0-or-1 output.spv");
    const std::string viewport=argv[3];
    if(viewport!="0"&&viewport!="1")throw std::runtime_error("explicit viewport profile required");
    const auto code=read(argv[1],16<<20),ctrl=read(argv[2],0x880);
    if(ctrl.size()!=0x880||word(ctrl,0)!=0x98761234||word(code,0)!=0x12345678||word(ctrl,0x704)!=code.size())throw std::runtime_error("invalid NVN shader control/code");
    const auto start=word(ctrl,0x708),size=word(ctrl,0x6f8),constant_start=word(ctrl,0x700),constant_size=word(ctrl,0x6fc);
    if(start>code.size()||size>code.size()-start||size<88||size%8||constant_start>code.size()||constant_size>code.size()-constant_start||constant_size%4)throw std::runtime_error("invalid shader/constant extent");
    Shader::ProgramHeader header{};std::memcpy(&header,code.data()+start,sizeof header);
    const unsigned tag=header.common0.shader_type;
    if(tag!=1&&tag!=5)throw std::runtime_error("only vertexB/fragment supported");
    std::vector<native_shaders::NativeCbufWord> constants;
    for(unsigned offset=0;offset<constant_size;offset+=4)constants.push_back({1,offset,word(code,constant_start+offset)});
    native_shaders::NativeInput input;
    input.stage=tag==1?native_shaders::NativeStage::VertexB:native_shaders::NativeStage::Fragment;
    input.source=std::span(code).subspan(start,size);input.start_address=start;
    input.local_memory_size=header.LocalMemorySize();input.viewport_transform=viewport=="1"?1:0;
    input.texture_bound_bank=2;input.proprietary_driver=true;input.cbuf_words=constants;
    auto env=native_shaders::Snapshot::FromNativeInput(input);
    Shader::ObjectPool<Shader::IR::Inst> inst;Shader::ObjectPool<Shader::IR::Block> blocks;Shader::ObjectPool<Shader::Maxwell::Flow::Block> flow;
    Shader::HostTranslateInfo host{};host.ApplyDescriptorLimitPolicy();
    Shader::Maxwell::Flow::CFG cfg(env,flow,env.StartAddress()+80,false);
    auto program=Shader::Maxwell::TranslateProgram(inst,blocks,env,cfg,host);
    // Diagnostic profile is explicit, never evidence of matching draw state.
    Shader::RuntimeInfo runtime{};runtime.input_topology=Shader::InputTopology::Triangles;
    runtime.generic_input_types.fill(Shader::AttributeType::Float);runtime.color_output_types.fill(Shader::AttributeType::Float);runtime.previous_stage_stores.mask.set();
    Shader::Maxwell::ConvertLegacyToGeneric(program,runtime);
    Shader::Profile profile{};profile.unified_descriptor_binding=true;profile.need_fastmath_off=true;
    Shader::Backend::Bindings bindings{};
    if (!program.info.storage_buffers_descriptors.empty() || !program.info.texture_descriptors.empty() ||
        !program.info.texture_buffer_descriptors.empty() || !program.info.image_descriptors.empty() ||
        !program.info.image_buffer_descriptors.empty()) throw std::runtime_error("resource mapping beyond uniform buffers is not integrated");
    auto spv=Shader::Backend::SPIRV::EmitSPIRV(profile,runtime,program,bindings);
    // Match the native runtime's per-stage uniform binding range; retain names.
    std::map<uint32_t,unsigned> uniform_ids;
    for (size_t at=5;at<spv.size();) {
        unsigned count=spv[at]>>16,op=spv[at]&65535;
        if (!count||count>spv.size()-at) throw std::runtime_error("malformed emitted instruction");
        if(op==5&&count>=3) {
            const char* text=reinterpret_cast<const char*>(&spv[at+2]);
            const size_t limit=(count-2)*4;size_t length=0;while(length<limit&&text[length])++length;
            if(length==limit)throw std::runtime_error("unterminated emitted name");
            if(length>1&&text[0]=='c') {
                unsigned bank=0;auto [end,error]=std::from_chars(text+1,text+length,bank);
                if(error==std::errc{}&&end==text+length&&bank<18)uniform_ids.emplace(spv[at+1],bank);
            }
        }
        at+=count;
    }
    if(uniform_ids.size()!=program.info.constant_buffer_descriptors.size())throw std::runtime_error("uniform variable mapping mismatch");
    unsigned remapped=0;
    for(size_t at=5;at<spv.size();at+=spv[at]>>16) {
        if((spv[at]&65535)==71&&(spv[at]>>16)==4&&uniform_ids.contains(spv[at+1])) {
            if(spv[at+2]==33) {spv[at+3]=(tag==5?18:0)+1+uniform_ids.at(spv[at+1]);++remapped;}
            if(spv[at+2]==34)spv[at+3]=0;
        }
    }
    if(remapped!=uniform_ids.size())throw std::runtime_error("uniform decoration mapping mismatch");
    std::ofstream f(argv[4],std::ios::binary);f.write(reinterpret_cast<const char*>(spv.data()),spv.size()*4);
    if(!f)throw std::runtime_error("output write failed");
    std::cout<<"{\"offline_diagnostic\":true,\"stage_tag\":"<<tag<<",\"words\":"<<spv.size()<<",\"uniforms\":"<<program.info.constant_buffer_descriptors.size()<<",\"storage\":"<<program.info.storage_buffers_descriptors.size()<<",\"textures\":"<<program.info.texture_descriptors.size()<<",\"native_game_frame\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
