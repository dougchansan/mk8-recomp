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
#include <sstream>
#include <filesystem>
#include <set>
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
    if(argc<5||argc>37)throw std::runtime_error("Usage: offline_nvn_shader code.bin ctrl.bin viewport-transform-0-or-1 output.spv");
    const std::string viewport=argv[3];
    if(viewport!="0"&&viewport!="1")throw std::runtime_error("explicit viewport profile required");
    if(argc>5&&viewport!="1")throw std::runtime_error("textured profile currently requires viewport transform enabled");
    const auto code=read(argv[1],16<<20),ctrl=read(argv[2],0x880);
    if((ctrl.size()<0x758||ctrl.size()>0x880)||word(ctrl,0)!=0x98761234||word(code,0)!=0x12345678||word(ctrl,0x704)!=code.size())throw std::runtime_error("invalid NVN shader control/code");
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
    auto control_env=native_shaders::Snapshot::FromNativeInput(input);
    std::vector<native_shaders::NativeTexture> textures;
    std::set<unsigned> slots;
    std::map<unsigned,Shader::TextureType> slot_types;
    std::map<unsigned,bool> slot_shadow;
    for(int arg=5;arg<argc;++arg) {
        const std::string text=argv[arg];
        if(!text.starts_with("2:"))throw std::runtime_error("profile requires driver texture bank2 and decimal byte offset");
        const auto suffix=text.find(':',2);const auto numeric_end=suffix==std::string::npos?text.size():suffix;
        const bool cube=suffix!=std::string::npos&&text.substr(suffix)==":cube";
        const bool array_shadow=suffix!=std::string::npos&&text.substr(suffix)==":array2d-shadow";
        const bool shadow=array_shadow||(suffix!=std::string::npos&&text.substr(suffix)==":2d-shadow");
        const bool array_color=suffix!=std::string::npos&&text.substr(suffix)==":array2d";
        const bool cube_array=suffix!=std::string::npos&&text.substr(suffix)==":cube-array";
        if(suffix!=std::string::npos&&!cube&&!shadow&&!array_color&&!cube_array)throw std::runtime_error("unsupported sampled profile type");
        unsigned offset=0;auto [end,error]=std::from_chars(text.data()+2,text.data()+numeric_end,offset);
        if(error!=std::errc{}||end!=text.data()+numeric_end||offset<0x20||offset>0x118||(offset-0x20)%8||!slots.insert(offset).second)
            throw std::runtime_error("invalid or duplicate normalized2D float profile slot");
        const uint32_t symbol=1+(offset-0x20)/8;
        constants.push_back({2,offset,symbol});
        const auto type=cube_array?Shader::TextureType::ColorArrayCube:((array_shadow||array_color)?Shader::TextureType::ColorArray2D:(cube?Shader::TextureType::ColorCube:Shader::TextureType::Color2D));
        slot_types.emplace(offset,type);slot_shadow.emplace(offset,shadow);
        textures.push_back({symbol,type,native_shaders::NativePixelFormat::RGBA8Unorm});
    }
    input.cbuf_words=constants;input.textures=textures;
    auto env=native_shaders::Snapshot::FromNativeInput(input);
    Shader::ObjectPool<Shader::IR::Inst> inst;Shader::ObjectPool<Shader::IR::Block> blocks;Shader::ObjectPool<Shader::Maxwell::Flow::Block> flow;
    Shader::HostTranslateInfo host{};host.ApplyDescriptorLimitPolicy();
    // CFG uses the original constant environment: type symbols cannot resolve a BRX table.
    Shader::Maxwell::Flow::CFG cfg(control_env,flow,control_env.StartAddress()+80,false);
    auto program=Shader::Maxwell::TranslateProgram(inst,blocks,env,cfg,host);
    // Diagnostic profile is explicit, never evidence of matching draw state.
    Shader::RuntimeInfo runtime{};runtime.input_topology=Shader::InputTopology::Triangles;
    runtime.generic_input_types.fill(Shader::AttributeType::Float);runtime.color_output_types.fill(Shader::AttributeType::Float);runtime.previous_stage_stores.mask.set();
    Shader::Maxwell::ConvertLegacyToGeneric(program,runtime);
    Shader::Profile profile{};profile.unified_descriptor_binding=true;profile.need_fastmath_off=true;
    Shader::Backend::Bindings bindings{};
    if (!program.info.texture_buffer_descriptors.empty() || !program.info.image_descriptors.empty() ||
        !program.info.image_buffer_descriptors.empty()) throw std::runtime_error("resource mapping beyond uniform buffers is not integrated");
    for(const auto& descriptor:program.info.constant_buffer_descriptors)
        if(descriptor.count!=1)throw std::runtime_error("uniform arrays require further integration");
    for(const auto& descriptor:program.info.texture_descriptors)
        if(slots.empty()||descriptor.count!=1||descriptor.cbuf_index!=2||!slots.contains(descriptor.cbuf_offset)||descriptor.type!=slot_types.at(descriptor.cbuf_offset)||descriptor.is_depth!=slot_shadow.at(descriptor.cbuf_offset))
            throw std::runtime_error("sampled profile mismatch: type="+std::to_string(static_cast<unsigned>(descriptor.type))+" depth="+std::to_string(descriptor.is_depth)+" count="+std::to_string(descriptor.count)+" bank="+std::to_string(descriptor.cbuf_index)+" offset="+std::to_string(descriptor.cbuf_offset));
    for(const auto& descriptor:program.info.storage_buffers_descriptors) {
        const unsigned base=tag==5?0x510:0x110;
        if(descriptor.is_written||descriptor.count!=1||descriptor.cbuf_index!=0||descriptor.cbuf_offset<base||descriptor.cbuf_offset>=base+0x100||(descriptor.cbuf_offset-base)%16)throw std::runtime_error("graphics storage descriptor is outside readonly native table");
    }
    auto spv=Shader::Backend::SPIRV::EmitSPIRV(profile,runtime,program,bindings);
    if(bindings.unified!=program.info.constant_buffer_descriptors.size()+program.info.storage_buffers_descriptors.size()+program.info.texture_descriptors.size())throw std::runtime_error("unsupported descriptor binding layout");
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
    std::map<uint32_t,unsigned> sampled_ids,storage_ids;
    for(size_t at=5;at<spv.size();at+=spv[at]>>16) {
        if((spv[at]&65535)==71&&(spv[at]>>16)==4&&spv[at+2]==33&&!uniform_ids.contains(spv[at+1])) {
            const auto old_binding=spv[at+3],first=unsigned(uniform_ids.size());
            if(old_binding<first)throw std::runtime_error("unexpected resource binding");
            const auto storage_count=program.info.storage_buffers_descriptors.size();
            if(old_binding-first<storage_count)storage_ids.emplace(spv[at+1],old_binding-first);
            else if(old_binding-first-storage_count<program.info.texture_descriptors.size())sampled_ids.emplace(spv[at+1],unsigned(old_binding-first-storage_count));
            else throw std::runtime_error("unexpected sampled binding");
        }
    }
    if(storage_ids.size()!=program.info.storage_buffers_descriptors.size()||sampled_ids.size()!=program.info.texture_descriptors.size())throw std::runtime_error("sampled variable mapping mismatch");
    unsigned remapped=0;
    for(size_t at=5;at<spv.size();at+=spv[at]>>16) {
        if((spv[at]&65535)==71&&(spv[at]>>16)==4&&uniform_ids.contains(spv[at+1])) {
            if(spv[at+2]==33) {spv[at+3]=(tag==5?18:0)+1+uniform_ids.at(spv[at+1]);++remapped;}
            if(spv[at+2]==34)spv[at+3]=0;
        }
    }
    if(remapped!=uniform_ids.size())throw std::runtime_error("uniform decoration mapping mismatch");
    std::vector<uint32_t> named(spv.begin(),spv.begin()+5);
    for(size_t at=5;at<spv.size();at+=spv[at]>>16) {
        const auto count=spv[at]>>16,op=spv[at]&65535;
        if(op==71&&count==4&&sampled_ids.contains(spv[at+1])) {
            if(spv[at+2]==33)spv[at+3]=(tag==5?128:0)+sampled_ids.at(spv[at+1]);
            if(spv[at+2]==34)spv[at+3]=2;
        }
        if(op==71&&count==4&&storage_ids.contains(spv[at+1])) {
            if(spv[at+2]==33)spv[at+3]=(tag==5?16:0)+storage_ids.at(spv[at+1]);
            if(spv[at+2]==34)spv[at+3]=1;
        }
        if(op==5&&count>=3&&storage_ids.contains(spv[at+1])) {
            const auto text=std::string(tag==5?"fp_s":"vp_s")+std::to_string(storage_ids.at(spv[at+1]));const auto words=(text.size()+1+3)/4;auto begin=named.size();named.resize(begin+2+words,0);named[begin]=uint32_t((words+2)<<16)|5;named[begin+1]=spv[at+1];std::memcpy(named.data()+begin+2,text.c_str(),text.size());
        } else if(op==5&&count>=3&&sampled_ids.contains(spv[at+1])) {
            const auto& descriptor=program.info.texture_descriptors.at(sampled_ids.at(spv[at+1]));
            std::ostringstream name;name<<(tag==5?"fp":"vp")<<(descriptor.is_depth?(descriptor.type==Shader::TextureType::ColorArray2D?"_tArrayShadow_cb2_":"_tShadow_cb2_"):(descriptor.type==Shader::TextureType::ColorArrayCube?"_tCubeArray_cb2_":(descriptor.type==Shader::TextureType::ColorArray2D?"_tArray_cb2_":(descriptor.type==Shader::TextureType::ColorCube?"_tCube_cb2_":"_t_cb2_"))))<<std::hex<<descriptor.cbuf_offset/4;
            const auto text=name.str();const auto words=(text.size()+1+3)/4;
            const auto begin=named.size();named.resize(begin+2+words,0);
            named[begin]=uint32_t((words+2)<<16)|5;named[begin+1]=spv[at+1];
            std::memcpy(named.data()+begin+2,text.c_str(),text.size());
        } else named.insert(named.end(),spv.begin()+at,spv.begin()+at+count);
    }
    spv=std::move(named);
    if(!sampled_ids.empty()||!storage_ids.empty()) {
        auto path=std::filesystem::path(argv[4]);path.replace_extension(".profile");
        std::ofstream metadata(path,std::ios::binary);metadata<<(storage_ids.empty()?"NVN_NATIVE_PROFILE_V7 normalized-sampled-float-arrays viewport1 triangles float-attributes\n":"NVN_NATIVE_PROFILE_V9 readonly-graphics-buffers-arrays viewport1 triangles float-attributes\n");
        if(!metadata)throw std::runtime_error("profile output write failed");
    }
    auto resource_path=std::filesystem::path(argv[4]);resource_path.replace_extension(".resources");std::ofstream resource_file(resource_path);
    for(const auto& [id,bank]:uniform_ids)resource_file<<"cb"<<bank<<"=s0b"<<(tag==5?18:0)+1+bank<<' ';
    for(const auto& [id,index]:storage_ids){const auto& descriptor=program.info.storage_buffers_descriptors.at(index);resource_file<<"sb[c0+0x"<<std::hex<<descriptor.cbuf_offset/4<<std::dec<<"]=s1b"<<(tag==5?16:0)+index<<' ';}
    for(const auto& [id,index]:sampled_ids){const auto& descriptor=program.info.texture_descriptors.at(index);resource_file<<"texTexture2D[c2:"<<descriptor.cbuf_offset/4<<"]=s2b"<<(tag==5?128:0)+index<<' ';}
    if(!resource_file)throw std::runtime_error("graphics resource output failed");
    std::ofstream f(argv[4],std::ios::binary);f.write(reinterpret_cast<const char*>(spv.data()),spv.size()*4);
    if(!f)throw std::runtime_error("output write failed");
    std::cout<<"{\"offline_diagnostic\":true,\"stage_tag\":"<<tag<<",\"words\":"<<spv.size()<<",\"uniforms\":"<<program.info.constant_buffer_descriptors.size()<<",\"storage\":"<<program.info.storage_buffers_descriptors.size()<<",\"textures\":"<<program.info.texture_descriptors.size()<<",\"native_game_frame\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
