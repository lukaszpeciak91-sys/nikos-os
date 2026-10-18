#!/usr/bin/env python3
"""Host compile/run tests against the production converter's pure logic."""
from pathlib import Path
import shutil, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
compiler=shutil.which("g++") or shutil.which("clang++")
if not compiler: raise SystemExit("SKIP: C++17 host compiler unavailable")
source=r"""
#include <cassert>
#include <cmath>
#include <cstring>
#include "converter/conversion.hpp"
using namespace nikos::converter;
bool close(double a,double b,double tolerance=1e-6){return std::fabs(a-b)<=tolerance;}
int main(){
 double base=0,out=0;
 assert(to_base(Category::Temperature,0,0,base)&&close(base,273.15));
 assert(from_base(Category::Temperature,1,base,out)&&close(out,32));
 assert(from_base(Category::Temperature,2,base,out)&&close(out,273.15));
 assert(to_base(Category::Temperature,1,212,base)&&close(base,373.15));
 assert(to_base(Category::Temperature,2,0,base)&&close(base,0));
 assert(to_base(Category::Temperature,0,-273.15,base));
 assert(!to_base(Category::Temperature,0,-273.2,base));
 assert(!to_base(Category::Temperature,1,-460,base));
 assert(!to_base(Category::Temperature,2,-0.1,base));
 assert(!to_base(Category::Length,2,-1,base));
 assert(to_base(Category::Length,3,1,base)&&close(base,1000));
 assert(from_base(Category::Length,6,base,out)&&close(out,0.621371192237,1e-8));
 assert(to_base(Category::Length,4,1,base)&&close(base,.0254));
 assert(to_base(Category::Length,5,1,base)&&close(base,.3048));
 assert(to_base(Category::Mass,3,1,base)&&close(base,.45359237));
 assert(to_base(Category::Mass,2,1,base)&&close(base,.028349523125));
 assert(to_base(Category::Volume,2,1,base)&&close(base,3.785411784));
 assert(to_base(Category::Volume,0,250,base)&&close(base,.25));
 assert(!to_base(Category::Mass,0,INFINITY,base));
 assert(!to_base(Category::Mass,0,NAN,base));
 assert(!to_base(Category::Mass,0,1e100,base));
 for(int c=0;c<4;++c){
   auto cat=static_cast<Category>(c);
   for(std::uint8_t i=0;i<count(cat);++i){
     const auto& unit=units(cat)[i];
     assert(unit.first<count(cat)&&unit.second<count(cat));
     assert(unit.first!=i&&unit.second!=i&&unit.first!=unit.second);
   }
 }
 assert(temperatures[0].first==1&&temperatures[0].second==2);
 assert(lengths[3].first==2&&lengths[3].second==6);
 assert(masses[1].first==0&&masses[1].second==3);
 assert(volumes[1].first==0&&volumes[1].second==2);
 assert(to_base(Category::Length,2,1.5,base));
 assert(from_base(Category::Length,4,base,out)&&close(out,59.0551181102));
 Editor e;e.begin(20.0,true);assert(e.value()==20.0);
 e.increment();assert(e.negative);e.increment();assert(!e.negative);
 for(int i=0;i<5;++i)assert(!e.next());
 assert(e.next()); // commit position
 e.begin(.5,false);assert(close(e.value(),.5));
 e.increment();assert(close(e.value(),1000.5)); // most significant digit
 // Visual character positions: sign, four digits, separator, tenths.
 char label[9]{};
 e.begin(-20.5,true);e.text(label);
 assert(std::strcmp(label,"-0020.5")==0);
 assert(e.character_index()==0);
 e.increment();e.text(label);assert(label[0]=='+');
 for(std::uint8_t i=0;i<6;++i) {
   assert(e.character_index()==(i==0?0:i==5?6:i));
   if(i<5)assert(!e.next());
 }
 assert(e.next());
 e.begin(1.5,false);e.text(label);
 assert(std::strcmp(label,"0001.5")==0);
 for(std::uint8_t i=0;i<5;++i) {
   assert(e.character_index()==(i==4?5:i));
   if(i<4)assert(!e.next());
 }
 assert(e.next());
 // Rejected value must not overwrite committed base.
 assert(to_base(Category::Temperature,0,20.,base));
 const double committed=base;
 e.begin(-274.,true);
 double candidate=0;
 assert(!to_base(Category::Temperature,0,e.value(),candidate));
 assert(base==committed);
 e.increment(); // correct sign without losing digits
 assert(e.value()==274.);
 // Discarding the local editor preserves the committed value.
 assert(base==committed);
 // Editor admission must never clamp a displayed physical quantity.
 assert(editable_value(Category::Temperature,-9999.9));
 assert(editable_value(Category::Temperature,9999.9));
 assert(!editable_value(Category::Temperature,-10000.0));
 assert(!editable_value(Category::Temperature,10000.0));
 assert(!editable_value(Category::Temperature,INFINITY));
 assert(!editable_value(Category::Mass,-0.1));
 assert(editable_value(Category::Length,0.0));
 assert(editable_value(Category::Length,9999.9));
 assert(!editable_value(Category::Length,10000.0));
 // Large physical quantity in mm cannot be edited; switch to m without
 // changing the base quantity, then return to mm and observe the same value.
 assert(to_base(Category::Length,2,20.0,base));
 const double original_base=base;
 assert(from_base(Category::Length,0,base,out));
 assert(close(out,20000.0));
 assert(!editable_value(Category::Length,out));
 assert(base==original_base);
 assert(from_base(Category::Length,2,base,out));
 assert(close(out,20.0));
 assert(editable_value(Category::Length,out));
 Editor restored;restored.begin(out,false);
 assert(close(restored.value(),20.0));
 assert(base==original_base);
 assert(from_base(Category::Length,0,base,out));
 assert(close(out,20000.0)); // unchanged after switching back
 // Negative temperature outside the range can be edited after a unit switch.
 assert(to_base(Category::Temperature,0,-9999.9,base)==false);
 assert(to_base(Category::Temperature,2,6000,base));
 assert(from_base(Category::Temperature,1,base,out));
 assert(!editable_value(Category::Temperature,out));
 assert(from_base(Category::Temperature,2,base,out));
 assert(editable_value(Category::Temperature,out));
 char formatted[24];
 format(100.,formatted);assert(std::strcmp(formatted,"100")==0);
 format(0.00000001,formatted);assert(std::strcmp(formatted,"1.00e-08")==0);
 format(INFINITY,formatted);assert(std::strcmp(formatted,"POZA ZAKR.")==0);
 format(1e12,formatted);assert(std::strcmp(formatted,"POZA ZAKR.")==0);
 return 0;
}
"""
with tempfile.TemporaryDirectory() as temp:
    src=Path(temp)/"converter.cpp";exe=Path(temp)/"converter"
    src.write_text(source,encoding="utf-8")
    subprocess.run([compiler,"-std=c++17","-Wall","-Wextra","-Werror",
                    "-I",str(root/"apps/converter/include"),str(src),
                    "-o",str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
main=(root/"main/app_main.cpp").read_text(encoding="utf-8")
launcher=(root/"components/launcher/launcher.cpp").read_text(encoding="utf-8")
app=(root/"apps/converter/converter_app.cpp").read_text(encoding="utf-8")
assert "RuntimeState::Converter" in main
assert "Action::OpenConverter" in main
assert "converter.begin();" in main and "converter.end();" in main
assert "converter.suspend();" in main and "converter.resume();" in main
assert "converter.update(input)" in main
assert "return Action::OpenConverter;" in launcher
assert '"KONWERTER"' in app  # Title belongs to ConverterApp; Launcher routes by Action.
guard=main.split("bool is_guarded_entertainment(",1)[1].split("bool trusted_civil_minute",1)[0]
assert "Converter" not in guard
assert "entertainment_guard" not in app
assert "Screen::Editing" in app and "editor_.increment()" in app
assert "screen_=Screen::Categories" in app
print("Converter production logic and routing guards PASS")

assert "font_.advance(" in app
assert "editor_.character_index()" in app
assert "editor_.text(edit)" in app
assert 'invalid_temperature_=true;editor_.cursor=0;' in app
assert '"PONIŻEJ ZERA ABS."' in app
assert "invalid_temperature_=false;render();return Result::Running;" in app
assert "if(screen_==Screen::Editing)" in app
assert "SIDE DŁUGO: ANULUJ" in app

assert "editable_value(category_,value)" in app
assert '"ZA DUŻA WARTOŚĆ"' in app
assert "editable_value(category_,input)" in app
assert "editor_.begin(value,category_==Category::Temperature)" in app
assert "unit_=static_cast<std::uint8_t>((unit_+1)%count(category_))" in app
