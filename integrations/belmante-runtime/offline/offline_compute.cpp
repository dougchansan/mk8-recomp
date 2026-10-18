// SPDX-License-Identifier: GPL-3.0-or-later
// Bounded offline SDK53 raw compute translation. No GPU or guest modules.
#include "shader_recompiler/environment.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/frontend/maxwell/translate_program.h"
#include "shader_recompiler/host_translate_info.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <map>
#include <charconv>
#include <filesystem>
#include <sstream>
std::vector<uint8_t> read(const char* name,size_t limit){std::ifstream f(name,std::ios::binary|std::ios::ate);auto n=f.tellg();if(!f||n<0||uint64_t(n)>limit)throw std::runtime_error("input size/read failure");std::vector<uint8_t>b(static_cast<size_t>(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))throw std::runtime_error("input truncated");return b;}
uint32_t word(const std::vector<uint8_t>& b,size_t offset){if(offset>b.size()||b.size()-offset<4)throw std::runtime_error("metadata read out of bounds");uint32_t value;std::memcpy(&value,b.data()+offset,4);return value;}
class ComputeEnvironment final:public Shader::Environment {
    std::vector<uint8_t> code_;uint32_t program_,constant_,constant_size_,shared_;std::array<uint32_t,3> group_;std::map<unsigned,uint32_t> texture_symbols_;std::map<unsigned,Shader::TextureType> texture_types_;
public:
    ComputeEnvironment(const std::vector<uint8_t>& code,const std::vector<uint8_t>& control,const std::map<unsigned,uint32_t>& symbols={},const std::map<unsigned,Shader::TextureType>& types={}):code_(code),texture_symbols_(symbols),texture_types_(types){
        if(control.size()<0x758||control.size()>0x880||word(control,0)!=0x98761234||word(control,0x704)!=code.size()||word(control,0x708)!=0)throw std::runtime_error("invalid raw compute control");
        program_=word(control,0x6f8);constant_=word(control,0x700);constant_size_=word(control,0x6fc);shared_=word(control,0x754);
        if(program_<16||program_%8||program_>code.size()||constant_>code.size()||constant_size_>code.size()-constant_||constant_size_%4)throw std::runtime_error("invalid compute extent");
        for(unsigned i=0;i<3;++i)group_[i]=word(control,0x748+i*4);
        if(!group_[0]||!group_[1]||!group_[2]||group_[0]>1024||group_[1]>1024||group_[2]>64||uint64_t(group_[0])*group_[1]*group_[2]>1024||shared_>65536||shared_%4)throw std::runtime_error("unsupported compute workgroup/shared size");
        stage=Shader::Stage::Compute;start_address=0;is_proprietary_driver=true;
    }
    u64 ReadInstruction(u32 address)override{if(address%8||address>program_||program_-address<8)throw std::runtime_error("compute instruction read out of bounds");u64 value;std::memcpy(&value,code_.data()+address,8);return value;}
    u32 ReadCbufValue(u32 bank,u32 offset)override{if(bank==2){auto found=texture_symbols_.find(offset);if(found!=texture_symbols_.end())return found->second;}if(bank!=1||offset%4||offset>constant_size_||constant_size_-offset<4)throw std::runtime_error("missing compute constant metadata bank="+std::to_string(bank)+" offset="+std::to_string(offset));return word(code_,constant_+offset);}
    Shader::TextureType ReadTextureType(u32 handle)override{for(const auto& [offset,symbol]:texture_symbols_)if(symbol==handle)return texture_types_.at(offset);throw std::runtime_error("compute texture metadata unsupported");}
    Shader::TexturePixelFormat ReadTexturePixelFormat(u32 handle)override{ReadTextureType(handle);return Shader::TexturePixelFormat::A8B8G8R8_UNORM;}
    bool IsTexturePixelFormatInteger(u32 handle)override{ReadTextureType(handle);return false;}
    u32 ReadViewportTransformState()override{throw std::runtime_error("compute viewport query is invalid");}
    u32 TextureBoundBuffer()const override{if(texture_symbols_.empty())throw std::runtime_error("explicit compute texture bank2 profile required");return 2;}
    u32 LocalMemorySize()const override{return 0;}
    u32 SharedMemorySize()const override{return shared_;}
    std::array<u32,3> WorkgroupSize()const override{return group_;}
    bool HasHLEMacroState()const override{return false;}
    std::optional<Shader::ReplaceConstant> GetReplaceConstBuffer(u32,u32)override{return {};}
    void Dump(u64,u64)override{}
};
int main(int argc,char** argv){try{
    if(argc<4||argc>36)throw std::runtime_error("Usage: offline_compute own-code.bin own-control.bin output.spv");
    const auto code=read(argv[1],0x100000),control=read(argv[2],0x880);ComputeEnvironment control_env(code,control);std::map<unsigned,uint32_t> symbols;std::map<unsigned,Shader::TextureType> types;
    for(int argument=4;argument<argc;++argument){std::string text=argv[argument];unsigned offset=0;if(!text.starts_with("2:"))throw std::runtime_error("compute texture profile requires bank2");auto suffix=text.find(':',2);auto numeric_end=suffix==std::string::npos?text.size():suffix;bool volume=suffix!=std::string::npos&&text.substr(suffix)==":3d";if(suffix!=std::string::npos&&!volume)throw std::runtime_error("unsupported compute texture type");auto [end,error]=std::from_chars(text.data()+2,text.data()+numeric_end,offset);if(error!=std::errc{}||end!=text.data()+numeric_end||offset<0x20||offset>0x118||(offset-0x20)%8||!symbols.emplace(offset,1+(offset-0x20)/8).second)throw std::runtime_error("invalid compute texture profile slot");types.emplace(offset,volume?Shader::TextureType::Color3D:Shader::TextureType::Color2D);}
    ComputeEnvironment env(code,control,symbols,types);
    Shader::ObjectPool<Shader::IR::Inst> inst;Shader::ObjectPool<Shader::IR::Block> blocks;Shader::ObjectPool<Shader::Maxwell::Flow::Block> flow;Shader::HostTranslateInfo host{};host.ApplyDescriptorLimitPolicy();
    Shader::Maxwell::Flow::CFG cfg(control_env,flow,0,false);auto program=Shader::Maxwell::TranslateProgram(inst,blocks,env,cfg,host);
    if(program.info.uses_local_memory)throw std::runtime_error("compute local-memory size requires inspected metadata");
    if(!program.shared_memory_size)for(const auto* block:program.blocks)for(const auto& instruction:block->Instructions())if(Shader::IR::NameOf(instruction.GetOpcode()).find("Shared")!=std::string_view::npos)throw std::runtime_error("shared operation without shared memory");
    if(!program.info.texture_buffer_descriptors.empty()||!program.info.image_descriptors.empty()||!program.info.image_buffer_descriptors.empty())throw std::runtime_error("compute image/texture resources unsupported");
    for(const auto& descriptor:program.info.constant_buffer_descriptors)if(descriptor.count!=1)throw std::runtime_error("compute uniform arrays unsupported");
    for(const auto& descriptor:program.info.storage_buffers_descriptors){std::cout<<"storage cbuf="<<descriptor.cbuf_index<<" offset="<<descriptor.cbuf_offset<<" count="<<descriptor.count<<'\n';if(descriptor.count!=1||descriptor.cbuf_index!=0)throw std::runtime_error("compute storage mapping unsupported");}
    for(const auto& descriptor:program.info.texture_descriptors)if(descriptor.is_depth||descriptor.count!=1||descriptor.cbuf_index!=2||!symbols.contains(descriptor.cbuf_offset)||descriptor.type!=types.at(descriptor.cbuf_offset))throw std::runtime_error("compute sampled profile mismatch type="+std::to_string(static_cast<unsigned>(descriptor.type))+" depth="+std::to_string(descriptor.is_depth)+" offset="+std::to_string(descriptor.cbuf_offset));
    Shader::Profile profile{};profile.unified_descriptor_binding=true;profile.need_fastmath_off=true;Shader::RuntimeInfo runtime{};Shader::Backend::Bindings bindings{};
    auto spv=Shader::Backend::SPIRV::EmitSPIRV(profile,runtime,program,bindings);
    std::map<uint32_t,unsigned> uniforms,storage,sampled;unsigned uniform_count=unsigned(program.info.constant_buffer_descriptors.size()),storage_count=unsigned(program.info.storage_buffers_descriptors.size()),sampled_count=unsigned(program.info.texture_descriptors.size());
    for(size_t at=5;at<spv.size();at+=spv[at]>>16){auto n=spv[at]>>16,op=spv[at]&65535;if(!n||n>spv.size()-at)throw std::runtime_error("malformed emitted instruction");if(op==5&&n>=3){const char* text=reinterpret_cast<const char*>(&spv[at+2]);size_t limit=(n-2)*4,length=0;while(length<limit&&text[length])++length;if(length==limit)throw std::runtime_error("bad emitted name");unsigned bank=0;if(length>1&&text[0]=='c'){auto [end,error]=std::from_chars(text+1,text+length,bank);if(error==std::errc{}&&end==text+length&&bank<18)uniforms.emplace(spv[at+1],bank);}}}
    for(size_t at=5;at<spv.size();at+=spv[at]>>16)if((spv[at]&65535)==71&&(spv[at]>>16)==4&&spv[at+2]==33&&!uniforms.contains(spv[at+1])){unsigned binding=spv[at+3];if(binding<uniform_count)throw std::runtime_error("unexpected compute binding");if(binding-uniform_count<storage_count)storage.emplace(spv[at+1],binding-uniform_count);else if(binding-uniform_count-storage_count<sampled_count)sampled.emplace(spv[at+1],binding-uniform_count-storage_count);else throw std::runtime_error("unexpected compute sampled binding");}
    if(uniforms.size()!=uniform_count||storage.size()!=storage_count||sampled.size()!=sampled_count||bindings.unified!=uniform_count+storage_count+sampled_count)throw std::runtime_error("compute resource mapping mismatch");
    std::ostringstream resources;for(const auto& [id,bank]:uniforms)resources<<"cb"<<bank<<"=s0b"<<bank+1<<' ';for(const auto& [id,index]:storage){const auto& descriptor=program.info.storage_buffers_descriptors.at(index);resources<<"sb[c0+0x"<<std::hex<<descriptor.cbuf_offset/4<<std::dec<<"]=s1b"<<index<<' ';}
    for(const auto& [id,index]:sampled){const auto& descriptor=program.info.texture_descriptors.at(index);resources<<(descriptor.type==Shader::TextureType::Color3D?"texTexture3D[c2:":"texTexture2D[c2:")<<descriptor.cbuf_offset/4<<"]=s2b"<<index<<' ';}
    std::vector<uint32_t> named(spv.begin(),spv.begin()+5);
    for(size_t at=5;at<spv.size();at+=spv[at]>>16){auto n=spv[at]>>16,op=spv[at]&65535;if(op==71&&n==4){auto id=spv[at+1],decoration=spv[at+2];if(uniforms.contains(id)){if(decoration==33)spv[at+3]=uniforms.at(id)+1;if(decoration==34)spv[at+3]=0;}if(storage.contains(id)){if(decoration==33)spv[at+3]=storage.at(id);if(decoration==34)spv[at+3]=1;}if(sampled.contains(id)){if(decoration==33)spv[at+3]=sampled.at(id);if(decoration==34)spv[at+3]=2;}}
        if(op==5&&n>=3&&storage.contains(spv[at+1])){auto text="cp_s"+std::to_string(storage.at(spv[at+1]));size_t words=(text.size()+1+3)/4,begin=named.size();named.resize(begin+2+words,0);named[begin]=uint32_t((words+2)<<16)|5;named[begin+1]=spv[at+1];std::memcpy(named.data()+begin+2,text.c_str(),text.size());}else if(op==5&&n>=3&&sampled.contains(spv[at+1])){const auto& descriptor=program.info.texture_descriptors.at(sampled.at(spv[at+1]));std::ostringstream label;label<<(descriptor.type==Shader::TextureType::Color3D?"cp_t3D_cb2_":"cp_t_cb2_")<<std::hex<<descriptor.cbuf_offset/4;auto text=label.str();size_t words=(text.size()+1+3)/4,begin=named.size();named.resize(begin+2+words,0);named[begin]=uint32_t((words+2)<<16)|5;named[begin+1]=spv[at+1];std::memcpy(named.data()+begin+2,text.c_str(),text.size());}else named.insert(named.end(),spv.begin()+at,spv.begin()+at+n);}
    auto output=std::filesystem::path(argv[3]);std::ofstream file(output,std::ios::binary);file.write(reinterpret_cast<const char*>(named.data()),named.size()*4);if(!file)throw std::runtime_error("compute output write failed");auto metadata=output;metadata.replace_extension(".resources");std::ofstream record(metadata);record<<resources.str();metadata=output;metadata.replace_extension(".profile");std::ofstream mode(metadata,std::ios::binary);mode<<(sampled_count?"NVN_NATIVE_PROFILE_V6 raw-compute sampled-float local0\n":"NVN_NATIVE_PROFILE_V4 raw-compute local0\n");if(!record||!mode)throw std::runtime_error("compute metadata output failed");auto group=env.WorkgroupSize();std::cout<<"compute words="<<named.size()<<" group="<<group[0]<<','<<group[1]<<','<<group[2]<<" shared="<<env.SharedMemorySize()<<" uniforms="<<uniform_count<<" storage="<<storage_count<<" native_game_frame=0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
