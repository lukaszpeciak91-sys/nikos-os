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
        screen_=Screen::Categories;invalid_temperature_=false;render();return Result::Running;
    }
    if(screen_==Screen::Categories){
        if(input.secondary_short){selection_=static_cast<std::uint8_t>((selection_+1)%5);render();}
        else if(input.primary_short){if(selection_==4)return Result::ExitRequested;choose_category();render();}
    }else if(screen_==Screen::Conversion){
        if(input.secondary_short){unit_=static_cast<std::uint8_t>((unit_+1)%count(category_));render();}
        else if(input.primary_short){double value=0;
            if(!from_base(category_,unit_,base_,value)
                || !editable_value(category_,value))return Result::Running;
            editor_.begin(value,category_==Category::Temperature);invalid_temperature_=false;screen_=Screen::Editing;render();}
    }else if(screen_==Screen::Editing){
        if(input.secondary_short){editor_.increment();invalid_temperature_=false;render();}
        else if(input.primary_short){if(editor_.next()){
            double next=0;if(to_base(category_,unit_,editor_.value(),next)){base_=next;screen_=Screen::Conversion;invalid_temperature_=false;}
            else {invalid_temperature_=true;editor_.cursor=0;} // Keep all entered digits for correction.
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
    if(screen_==Screen::Editing){
        // The editor is the only numeric value on this screen. Its cursor
        // follows the precise glyph advances of the selected pre-rendered font.
        char edit[9]{};
        editor_.text(edit);
        constexpr std::int16_t kEditX=34;
        constexpr std::int16_t kEditY=44;
        font_.select(font_core::Mode::PreRendered);
        font_.set_size(font_core::Size::Readable);
        const auto selected=editor_.character_index();
        std::int16_t cursor_x=kEditX;
        for(std::uint8_t i=0;i<selected;++i)
            cursor_x=static_cast<std::int16_t>(cursor_x+font_.advance(
                static_cast<std::uint8_t>(edit[i])));
        const std::int16_t cursor_width=font_.advance(
            static_cast<std::uint8_t>(edit[selected]));
        font_.draw(edit,{kEditX,kEditY,190,24},
            board::DisplayColor::Accent,board::DisplayColor::Background);
        // A bright selected glyph plus a thicker underline distinguishes
        // the active position without changing fixed-digit editing.
        const char focused[] = {edit[selected], '\\0'};
        font_.draw(focused,{cursor_x,kEditY,cursor_width,24},
            board::DisplayColor::PrimaryText,board::DisplayColor::Background);
        board_.fill_rect(cursor_x,70,cursor_width,4,board::DisplayColor::Accent);
        draw(units(category_)[unit_].label,34,75,170);
        if(invalid_temperature_)
            draw("PONIŻEJ ZERA ABS.",12,96,216,board::DisplayColor::Attention);
        else
            draw("SIDE +1  M5 DALEJ",12,96,216);
        draw("SIDE DŁUGO: ANULUJ",12,116,216);
        return;
    }
    double input=0;
    if(!from_base(category_,unit_,base_,input))input=0;
    char number[24];format(input,number);
    char line[48];std::snprintf(line,sizeof(line),"%s %s",number,units(category_)[unit_].label);
    // Prefer the approved readable raster for numbers; fallback keeps long
    // scientific values and labels within the 240-pixel viewport.
    const auto draw_value = [&](const char* text, std::int16_t y,
        board::DisplayColor color) {
        font_.select(font_core::Mode::PreRendered);
        font_.set_size(font_core::Size::Readable);
        if (font_.measure(text, 216, 24).fits) {
            font_.draw(text,{12,y,216,24},color,board::DisplayColor::Background);
        } else {
            draw(text,12,y,216,color);
        }
    };
    draw_value(line,29,board::DisplayColor::Accent);
    const auto& u=units(category_)[unit_];
    for(std::uint8_t row=0;row<2;++row){
        const auto dest=row==0?u.first:u.second;
        double converted=0;char result[24];
        if(from_base(category_,dest,base_,converted))format(converted,result);
        else std::snprintf(result,sizeof(result),"POZA ZAKR.");
        char text[48];std::snprintf(text,sizeof(text),"%s %s",result,units(category_)[dest].label);
        draw_value(text,59+row*27,board::DisplayColor::SecondaryText);
    }
    draw(editable_value(category_,input) ? "M5 EDYTUJ  SIDE JEDN."
        : "ZA DUŻA WARTOŚĆ",12,117,216);
}
}
