#include "autocomplete.h"

namespace navitia { namespace autocomplete {

void compute_score_poi(const type::PT_Data &pt_data, const georef::GeoRef &georef) {
}


void compute_score_way(const type::PT_Data &pt_data, const georef::GeoRef &georef) {
}


void compute_score_stop_area(const type::PT_Data &pt_data, const georef::GeoRef &georef) {
}


void compute_score_stop_point(const type::PT_Data &pt_data, const georef::GeoRef &georef) {
}


void compute_score_admin(const type::PT_Data &pt_data, const georef::GeoRef &georef) {
}


void compute_score_line(const type::PT_Data &pt_data, const georef::GeoRef &georef) {
}

template<>
void Autocomplete<type::idx_t>::compute_score(const type::PT_Data &pt_data, const georef::GeoRef &georef,
                   const type::Type_e type) {
    switch(type){
        case type::Type_e::StopArea:
            compute_score_stop_area(pt_data, georef);
            break;
        case type::Type_e::StopPoint:
            compute_score_stop_point(pt_data, georef);
            break;
        case type::Type_e::Line:
            compute_score_line(pt_data, georef);
            break;
        case type::Type_e::Admin:
            compute_score_admin(pt_data, georef);
            break;
        case type::Type_e::Way:
            compute_score_way(pt_data, georef);
            break;
        case type::Type_e::POI:
            compute_score_poi(pt_data, georef);
            break;
        default:
            break;
    }
}


}}
