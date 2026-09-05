#include "spec/spec.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <variant>
#include <cstring>

namespace wise::spec {
namespace {
struct Value { using Object=std::map<std::string,Value>; using Array=std::vector<Value>; std::variant<std::nullptr_t,bool,std::int64_t,double,std::string,Object,Array> v; };
class Parser {
 public: explicit Parser(const std::string& s):s_(s){} Value parse(){ ws(); Value v=value(); ws(); if(i_!=s_.size()) fail(""); return v; }
 private:
  const std::string&s_; size_t i_{0};
  [[noreturn]] void fail(const std::string&p){throw SpecError({{p,"invalid JSON"}});}
  void ws(){while(i_<s_.size() && (s_[i_]==' '||s_[i_]=='\n'||s_[i_]=='\r'||s_[i_]=='\t'))++i_;}
  Value value(){ws(); if(i_>=s_.size())fail(""); char c=s_[i_]; if(c=='{')return object(); if(c=='[')return array(); if(c=='"')return Value{string()}; if(c=='t'&&take("true"))return Value{true}; if(c=='f'&&take("false"))return Value{false}; if(c=='n'&&take("null"))return Value{nullptr}; if(c=='-'||std::isdigit(static_cast<unsigned char>(c)))return number(); fail("");}
  bool take(const char* x){size_t n=std::strlen(x); if(s_.compare(i_,n,x)==0){i_+=n;return true;} return false;}
  std::string string(){ if(s_[i_++]!='"')fail(""); std::string o; while(i_<s_.size()){unsigned char c=s_[i_++]; if(c=='"')return o; if(c<0x20)fail(""); if(c=='\\'){if(i_>=s_.size())fail(""); char e=s_[i_++]; const char* esc="\"\\/bfnrt"; const char* q=std::strchr(esc,e); if(!q){if(e=='u'){if(i_+4>s_.size())fail(""); unsigned n=0; for(int k=0;k<4;k++){char h=s_[i_++]; n=n*16+(std::isdigit((unsigned char)h)?h-'0':(std::tolower((unsigned char)h)-'a'+10));} if(n<128)o.push_back((char)n); else fail("");}else fail("");} else o.push_back("\"\\/\b\f\n\r\t"[q-esc]);} else o.push_back((char)c);} fail("");}
  Value number(){size_t b=i_; if(s_[i_]=='-')++i_; if(i_>=s_.size())fail(""); if(s_[i_]=='0')++i_; else {if(!std::isdigit((unsigned char)s_[i_]))fail(""); while(i_<s_.size()&&std::isdigit((unsigned char)s_[i_]))++i_;} bool real=false; if(i_<s_.size()&&s_[i_]=='.'){real=true;++i_; if(i_>=s_.size()||!std::isdigit((unsigned char)s_[i_]))fail(""); while(i_<s_.size()&&std::isdigit((unsigned char)s_[i_]))++i_;} if(i_<s_.size()&&(s_[i_]=='e'||s_[i_]=='E')){real=true;++i_;if(i_<s_.size()&&(s_[i_]=='+'||s_[i_]=='-'))++i_;if(i_>=s_.size()||!std::isdigit((unsigned char)s_[i_]))fail("");while(i_<s_.size()&&std::isdigit((unsigned char)s_[i_]))++i_;} std::string n=s_.substr(b,i_-b); try {if(real)return Value{std::stod(n)}; return Value{static_cast<std::int64_t>(std::stoll(n))};}catch(...){fail("");}}
  Value object(){++i_; Value::Object o; ws(); if(i_<s_.size()&&s_[i_]=='}'){++i_;return Value{o};} while(true){ws();if(i_>=s_.size()||s_[i_]!='"')fail(""); std::string k=string(); ws();if(i_>=s_.size()||s_[i_++]!=':')fail(""); if(o.count(k))fail(""); o.emplace(std::move(k),value()); ws();if(i_>=s_.size())fail("");if(s_[i_]=='}'){++i_;return Value{o};}if(s_[i_++]!=',')fail("");}}
  Value array(){++i_; Value::Array a; ws();if(i_<s_.size()&&s_[i_]==']'){++i_;return Value{a};}while(true){a.push_back(value());ws();if(i_>=s_.size())fail("");if(s_[i_]==']'){++i_;return Value{a};}if(s_[i_++]!=',')fail("");}}
};
const Value::Object& obj(const Value&v,const std::string&p){auto q=std::get_if<Value::Object>(&v.v);if(!q)throw SpecError({{p,"expected object"}});return *q;}
const Value::Array& arr(const Value&v,const std::string&p){auto q=std::get_if<Value::Array>(&v.v);if(!q)throw SpecError({{p,"expected array"}});return *q;}
std::string str(const Value::Object&o,const std::string&k,const std::string&p){auto i=o.find(k);if(i==o.end())throw SpecError({{p+"/"+k,"missing required field"}});auto q=std::get_if<std::string>(&i->second.v);if(!q||q->empty())throw SpecError({{p+"/"+k,"expected non-empty string"}});return *q;}
std::string esc(const std::string&s){std::ostringstream o; o<<'"';for(unsigned char c:s){if(c=='"'||c=='\\')o<<'\\'<<c;else if(c=='\n')o<<"\\n";else if(c=='\r')o<<"\\r";else if(c=='\t')o<<"\\t";else o<<c;}return o<<'"',o.str();}
std::string canon(const Value&v){if(std::holds_alternative<std::nullptr_t>(v.v))return"null";if(auto q=std::get_if<bool>(&v.v))return*q?"true":"false";if(auto q=std::get_if<std::int64_t>(&v.v))return std::to_string(*q);if(auto q=std::get_if<double>(&v.v)){std::ostringstream o;o<<std::setprecision(17)<<*q;return o.str();}if(auto q=std::get_if<std::string>(&v.v))return esc(*q);if(auto q=std::get_if<Value::Array>(&v.v)){std::string o="[";for(size_t i=0;i<q->size();++i){if(i)o+=',';o+=canon((*q)[i]);}return o+"]";}std::string o="{";bool first=true;for(const auto&[k,x]:std::get<Value::Object>(v.v)){if(!first)o+=',';first=false;o+=esc(k)+":"+canon(x);}return o+"}";}
}

SpecError::SpecError(std::vector<Diagnostic> d):std::runtime_error(d.empty()?"spec error":d.front().pointer+": "+d.front().message),diagnostics_(std::move(d)){}
Document parse(const std::string&json){try {Value root=Parser(json).parse();const auto&o=obj(root,"");auto vi=o.find("version");if(vi==o.end()||!std::holds_alternative<std::int64_t>(vi->second.v)||std::get<std::int64_t>(vi->second.v)!=1)throw SpecError(std::vector<Diagnostic>{{"/version","unsupported version; expected integer 1"}});Document d;auto states=arr(o.at("states"),"/states");for(size_t i=0;i<states.size();++i)d.model.add_state({str(obj(states[i],"/states"),"id","/states/"+std::to_string(i))});d.model.set_initial_state(str(o,"initial_state",""));auto fs=arr(o.at("functions"),"/functions");for(const auto&x:fs){const auto&f=obj(x,"");model::Function fn;fn.id=str(f,"id","");for(const auto&y:arr(f.at("params"),"")){const auto&z=obj(y,"");fn.parameters.push_back({str(z,"name",""),str(z,"type","")});}for(const auto&y:arr(f.at("returns"),"")){const auto&z=obj(y,"");fn.returns.push_back({str(z,"name",""),str(z,"type","")});}d.model.add_function(std::move(fn));}auto ts=arr(o.at("transitions"),"/transitions");for(const auto&x:ts){const auto&t=obj(x,"");model::Transition tr{str(t,"id",""),str(t,"from",""),str(t,"to",""),str(t,"function",{})};if(auto y=t.find("expect");y!=t.end()){const auto&e=obj(y->second,"");tr.expect_present=true;tr.expect=str(e,"state","");}d.model.add_transition(std::move(tr));}auto lim=obj(o.at("limits"),"/limits");auto num=[&](const char*k){auto q=lim.find(k);if(q==lim.end()||!std::holds_alternative<std::int64_t>(q->second.v)||std::get<std::int64_t>(q->second.v)<=0)throw SpecError(std::vector<Diagnostic>{{std::string("/limits/")+k,"expected positive integer"}});return static_cast<size_t>(std::get<std::int64_t>(q->second.v));};d.model.set_limits({num("max_cases"),num("max_steps"),num("max_subprocesses")});auto seed=o.find("seed");if(seed==o.end()||!std::holds_alternative<std::int64_t>(seed->second.v)||std::get<std::int64_t>(seed->second.v)<0)throw SpecError(std::vector<Diagnostic>{{"/seed","expected unsigned integer"}});d.seed=static_cast<uint64_t>(std::get<std::int64_t>(seed->second.v));d.model.validate();d.canonical_json=canon(root);return d;} catch(const model::ModelError&e){throw SpecError(std::vector<Diagnostic>{{"","semantic validation failed: "+std::string(e.what())}});} }
std::string normalize(const std::string&json){return parse(json).canonical_json;}
}
