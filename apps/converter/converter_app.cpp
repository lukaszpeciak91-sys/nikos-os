#include "converter/converter_app.hpp"
#include <cstdio>
namespace nikos::converter {
namespace {
constexpr const char* kTitles[]={"TEMPERATURA","DŁUGOŚĆ","MASA","OBJĘTOŚĆ"};
constexpr double kInitial[]={293.15,1.,1.,1.};
}
void ConverterApp::begin(){active_=true;suspended_=false;screen_=Screen::Categories;selection_=0;render();}
void ConverterApp::redraw(){if(active_&&!suspended_)render();}
void ConverterApp::draw(const char* text,std::int16_t x,std::int16_t y,std::int16_t w,board::DisplayColor color){
    font_.select(font_core::Mode::PreRendered);font_.set_size(font_core::Size::Compact);
    font_.draw(text,{x,y,w,19},color,board::DisplayColor::Background);
}
void ConverterApp::choose_category(){category_=static_cast<Category>(selection_);unit_=0;base_=kInitial[selection_];screen_=Screen::Conversion;}
ConverterApp::Result ConverterApp::update(const board::InputState& input){
    if(!active_)return Result::ExitRequested;
    if(suspended_)return Result::Running;
    if(input.secondary_long){
        if(screen_==Screen::Categories)return Result::ExitRequested;
        screen_=Screen::Categories;render();return Result::Running;
    }
    if(screen_==Screen::Categories){
        if(input.secondary_short){selection_=static_cast<std::uint8_t>((selection_+1)%5);render();}
        else if(input.primary_short){if(selection_==4)return Result::ExitRequested;choose_category();render();}
    }else if(screen_==Screen::Conversion){
        if(input.secondary_short){unit_=static_cast<std::uint8_t>((unit_+1)%count(category_));render();}
        else if(input.primary_short){double value=0;if(!from_base(category_,unit_,base_,value))return Result::Running;
            editor_.begin(value,category_==Category::Temperature);screen_=Screen::Editing;render();}
    }else if(screen_==Screen::Editing){
        if(input.secondary_short){editor_.increment();render();}
        else if(input.primary_short){if(editor_.next()){
            double next=0;if(to_base(category_,unit_,editor_.value(),next)){base_=next;screen_=Screen::Conversion;}
            else {editor_.cursor=0;} // Invalid temperature: remain in editor.
            }render();}
    }
    return Result::Running;
}
void ConverterApp::render(){
    board_.clear_screen();
    if(screen_==Screen::Categories){
        draw("KONWERTER",14,5,210);
        const std::uint8_t first=selection_>=3?static_cast<std::uint8_t>(selection_-2):0;
        for(std::uint8_t slot=0;slot<3;++slot){
            const auto index=static_cast<std::uint8_t>(first+slot);
            const std::int16_t y=static_cast<std::int16_t>(31+slot*27);
            if(index==selection_)board_.fill_rect(16,y,208,23,board::DisplayColor::Surface);
            font_.select(font_core::Mode::PreRendered);font_.set_size(font_core::Size::Readable);
            font_.draw(index==4?"POWRÓT":kTitles[index],{22,y,195,24},
                index==selection_?board::DisplayColor::PrimaryText:board::DisplayColor::SecondaryText,
                index==selection_?board::DisplayColor::Surface:board::DisplayColor::Background);
        }
        board_.draw_text_region(12,119,218,14,"M5 WYBIERZ  SIDE DALEJ",1);
        return;
    }
    draw(kTitles[static_cast<std::uint8_t>(category_)],12,3,216);
    double input=0;
    if(!from_base(category_,unit_,base_,input))input=0;
    char number[24];format(input,number);
    char line[48];std::snprintf(line,sizeof(line),"%s %s",number,units(category_)[unit_].label);
    draw(line,12,31,216,board::DisplayColor::Accent);
    if(screen_==Screen::Editing){
        // Fixed positions are large enough to edit on the LCD; cursor is marked.
        char edit[24];std::snprintf(edit,sizeof(edit),"%c%c%c%c.%c",
            editor_.digits[0],editor_.digits[1],editor_.digits[2],editor_.digits[3],editor_.digits[4]);
        char typed[32];std::snprintf(typed,sizeof(typed),"%s%s",editor_.negative?"-":"",edit);
        draw(typed,13,61,210,board::DisplayColor::Accent);
        const auto cursor_x=static_cast<std::int16_t>(13+editor_.cursor*15);
        board_.fill_rect(cursor_x,84,12,3,board::DisplayColor::Accent);
        draw("SIDE +1  M5 DALEJ",12,104,216);
        return;
    }
    const auto& u=units(category_)[unit_];
    for(std::uint8_t row=0;row<2;++row){
        const auto dest=row==0?u.first:u.second;
        double converted=0;char result[24];
        if(from_base(category_,dest,base_,converted))format(converted,result);
        else std::snprintf(result,sizeof(result),"POZA ZAKR.");
        char text[48];std::snprintf(text,sizeof(text),"%s %s",result,units(category_)[dest].label);
        draw(text,12,61+row*24,216,board::DisplayColor::SecondaryText);
    }
    draw("M5 EDYTUJ  SIDE JEDN.",12,117,216);
}
}
