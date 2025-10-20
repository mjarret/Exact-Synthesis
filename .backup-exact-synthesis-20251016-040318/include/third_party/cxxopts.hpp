/*
Copyright (c) 2014-2022 Jarryd Beck
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifndef CXXOPTS_HPP_INCLUDED
#define CXXOPTS_HPP_INCLUDED
#include <cstring>
#include <exception>
#include <limits>
#include <initializer_list>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <algorithm>
#include <locale>
#ifndef CXXOPTS_NO_REGEX
#  include <regex>
#endif
// Minimal embedded cxxopts subset: enough for add_options, value<T>, parse, count, help
// NOTE: This is a slimmed header to avoid vendoring the full 2800-line file.
// For full features, replace with the official header.
namespace cxxopts {
struct OptionException : std::exception { std::string m; OptionException(std::string s):m(std::move(s)){} const char* what() const noexcept override { return m.c_str(); } };
class ValueBase { public: virtual ~ValueBase() = default; virtual void parse(const std::string&)=0; };
template <typename T> class Value : public ValueBase {
public:
  explicit Value(T& out):out_(out){}
  Value& default_value(const std::string& s){ has_default_=true; default_str_=s; return *this; }
  Value& implicit_value(const std::string& s){ implicit_=true; implicit_str_=s; return *this; }
  void parse(const std::string& s) override { from(s, out_); }
  bool has_default() const { return has_default_; }
  const std::string& default_str() const { return default_str_; }
  bool has_implicit() const { return implicit_; }
  const std::string& implicit_str() const { return implicit_str_; }
private:
  T& out_;
  bool has_default_{false};
  std::string default_str_{};
  bool implicit_{false};
  std::string implicit_str_{};
  static void from(const std::string& s, int& v){ v = std::stoi(s);} static void from(const std::string& s, std::string& v){ v = s;} static void from(const std::string& s, bool& v){ if(s.empty()) { v=true; return; } if(s=="1"||s=="true"||s=="on"||s=="yes") v=true; else if(s=="0"||s=="false"||s=="off"||s=="no") v=false; else v=true; }
};
template <typename T> inline Value<T> value(T& t){ return Value<T>(t);} 
struct Opt { std::string long_name; std::string help; std::unique_ptr<ValueBase> val; bool has_value=false; std::string default_val; bool implicit=false; std::string implicit_val; };
class Options {
public:
  Options(std::string prog, std::string help):prog_(std::move(prog)), help_(std::move(help)){}
  class AddProxy {
  public:
    explicit AddProxy(std::vector<Opt>& v):v_(v){}
    AddProxy& operator()(const std::string& name, const std::string& help){ add(name, help); return *this;}
    template <typename T> AddProxy& operator()(const std::string& name, const std::string& help, Value<T> val){ auto& o=add(name, help); if constexpr (std::is_same<T,bool>::value) { o.implicit=true; o.implicit_val="true"; } if(val.has_default()) o.default_val=val.default_str(); if(val.has_implicit()){ o.implicit=true; o.implicit_val=val.implicit_str(); } o.val=std::make_unique<Value<T>>(val); o.has_value=true; return *this; }
    template <typename T> AddProxy& operator()(const std::string& name, const std::string& help, Value<T> val, const std::string&){ auto& o=add(name, help); o.val=std::make_unique<Value<T>>(val); o.has_value=true; return *this; }
    AddProxy& implicit_value(const std::string& v){ if(!v_.empty()) v_.back().implicit=true, v_.back().implicit_val=v; return *this; }
    AddProxy& default_value(const std::string& v){ if(!v_.empty()) v_.back().default_val=v; return *this; }
  private:
    Opt& add(const std::string& name, const std::string& help){ Opt o; o.long_name=parse_name(name); o.help=help; v_.push_back(std::move(o)); return v_.back(); }
    static std::string parse_name(const std::string& n){ auto pos=n.find(','); return pos==std::string::npos?n:n.substr(pos+1); }
    std::vector<Opt>& v_;
  };
  AddProxy add_options(){ return AddProxy(opts_); }
  struct ParseResult { std::unordered_map<std::string,int> counts; bool count(const std::string& n) const { return counts.count(n)>0; } };
  ParseResult parse(int argc, char** argv){
    // preload defaults
    for(auto& o:opts_) if(!o.default_val.empty() && o.val) o.val->parse(o.default_val);
    ParseResult r; for(int i=1;i<argc;++i){ std::string a(argv[i]); if(a.rfind("--",0)==0){ auto name=a.substr(2); auto it=find(name); if(it==opts_.end()) throw OptionException("Unknown option --"+name); if(it->has_value){ std::string v; if(i+1<argc && argv[i+1][0]!='-') v=argv[++i]; else if(it->implicit) v=it->implicit_val; else throw OptionException("Missing value for --"+name); it->val->parse(v);} r.counts[name]++; } else if(a.rfind('-',0)==0 && a.size()>1){ // short not chained; map to long by scanning
        std::string sh=a.substr(1); bool consumed=false; for(auto& o:opts_){ if(!o.long_name.empty() && !sh.empty() && o.long_name[0]==sh[0]){ if(o.has_value){ std::string v; if(i+1<argc && argv[i+1][0]!='-') v=argv[++i]; else if(o.implicit) v=o.implicit_val; else throw OptionException("Missing value for -"+sh); o.val->parse(v);} r.counts[o.long_name]++; consumed=true; break; } } if(!consumed){ throw OptionException("Unknown option -"+sh);} }
      else { positionals_.push_back(a); }
    }
    return r;
  }
  std::string help() const { std::ostringstream oss; oss<<"Options:\n"; for(auto const& o:opts_){ oss<<"  --"<<o.long_name; if(o.has_value) oss<<" <value>"; if(!o.help.empty()) oss<<"\t"<<o.help; if(!o.default_val.empty()) oss<<" (default "<<o.default_val<<")"; oss<<"\n"; } return oss.str(); }
private:
  std::vector<Opt>::iterator find(const std::string& longname){ return std::find_if(opts_.begin(), opts_.end(), [&](const Opt& o){ return o.long_name==longname;}); }
  std::string prog_, help_;
  std::vector<Opt> opts_;
  std::vector<std::string> positionals_;
};
template <typename T> inline Value<T>* value(){ static_assert(!std::is_same<T,T>::value, "use value(ref) overload"); return nullptr; }
} // namespace cxxopts
#endif // CXXOPTS_HPP_INCLUDED
