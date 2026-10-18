#pragma once
#include <cstdint>

namespace nikos::vw_garage {
struct CatalogueEntry {
    const char* id;
    const char* name;
    std::uint16_t debut_year;
    const char* vehicle_type;
    const char* description_pl;
};

// Exact editorial content; documentation links are deliberately not firmware data.
inline constexpr CatalogueEntry kModels[25] = {
    {"beetle_type1_postwar", "Garbus (Typ 1)", 1945, "SEDAN", "Silnik z tyłu\nchłodzono powietrzem."},
    {"transporter_t1", "Transporter T1", 1950, "VAN", "Projekt zrodził się ze\nszkicu Bena Pona."},
    {"karmann_ghia_t14", "Karmann Ghia", 1955, "COUPE", "Włoskie nadwozie powstało\nna bazie Garbusa."},
    {"transporter_t2", "Transporter T2", 1967, "VAN", "Drugą generację Bulliego\nbudowano w Hanowerze."},
    {"passat_b1", "Passat B1", 1973, "SEDAN", "Pod jego nadwoziem kryła\nsię technika Audi 80."},
    {"scirocco_1", "Scirocco I", 1974, "COUPE", "Coupé pojawiło się\nprzed premierą Golfa."},
    {"golf_1", "Golf I", 1974, "HATCHBACK", "Zastąpił Garbusa i\npostawił na przedni napęd."},
    {"polo_1", "Polo I", 1975, "HATCHBACK", "Małe Polo bazowało na\nsamochodzie Audi 50."},
    {"transporter_t3", "Transporter T3", 1979, "VAN", "Był ostatnim Bullim\nz silnikiem z tyłu."},
    {"golf_2", "Golf II", 1983, "HATCHBACK", "W tej generacji pojawiły\nsię ABS i napęd Syncro."},
    {"passat_b3", "Passat B3", 1988, "SEDAN", "Jego gładki przód nie\nmiał klasycznej kraty."},
    {"transporter_t4", "Transporter T4", 1990, "VAN", "Jako pierwszy T miał\nsilnik i napęd z przodu."},
    {"golf_3", "Golf III", 1991, "HATCHBACK", "W tej generacji\npojawił się silnik VR6."},
    {"passat_b5", "Passat B5", 1996, "SEDAN", "Miał wzdłużny silnik i\nplatformę z Audi A4."},
    {"golf_4", "Golf IV", 1997, "HATCHBACK", "Klasyczny słupek C\nnawiązywał do Golfa I."},
    {"new_beetle_9c", "New Beetle", 1997, "HATCHBACK", "Styl retro produkowano\nw meksykańskiej Puebli."},
    {"touareg_1", "Touareg I", 2002, "SUV", "Pierwszy SUV VW miał stały\nnapęd na cztery koła."},
    {"transporter_t5", "Transporter T5", 2003, "VAN", "Piąta generacja Bulliego\notrzymała nowe nadwozie."},
    {"golf_5", "Golf V", 2003, "HATCHBACK", "Dostał niezależne\nzawieszenie tylne."},
    {"golf_6", "Golf VI", 2008, "HATCHBACK", "Oferował adaptacyjne\nzawieszenie DCC."},
    {"golf_7", "Golf VII", 2012, "HATCHBACK", "Platforma MQB pomogła\nobniżyć masę auta."},
    {"id3_1", "ID.3", 2019, "HATCHBACK", "Pierwszy model VW oparty\nna platformie MEB."},
    {"golf_8", "Golf VIII", 2019, "HATCHBACK", "Wprowadził cyfrowy kokpit\ni napędy hybrydowe."},
    {"id4_1", "ID.4", 2020, "SUV", "Pierwszy elektryczny\nSUV Volkswagena."},
    {"id_buzz_short", "ID. Buzz", 2022, "VAN", "Elektryczny van w\nstylu legendarnego T1."},
};
inline constexpr std::uint8_t kModelCount = 25;
}  // namespace nikos::vw_garage
