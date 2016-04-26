/* Copyright © 2001-2014, Canal TP and/or its affiliates. All rights reserved.
  
This file is part of Navitia,
    the software to build cool stuff with public transport.
 
Hope you'll enjoy and contribute to this project,
    powered by Canal TP (www.canaltp.fr).
Help us simplify mobility and open public transport:
    a non ending quest to the responsive locomotion way of traveling!
  
LICENCE: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Affero General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
   
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU Affero General Public License for more details.
   
You should have received a copy of the GNU Affero General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
  
Stay tuned using
twitter @navitia 
IRC #navitia on freenode
https://groups.google.com/d/forum/navitia
www.navitia.io
*/

#include "autocomplete.h"
#include "type/pt_data.h"
namespace navitia { namespace autocomplete {

static void compute_score_poi(type::PT_Data&, georef::GeoRef& georef) {
    for (auto it = georef.fl_poi.word_quality_list.begin(); it != georef.fl_poi.word_quality_list.end(); ++it){
        for (navitia::georef::Admin* admin : georef.pois[it->first]->admin_list){
            if(admin->level == 8){
                it->second.score = georef.fl_admin.word_quality_list.at(admin->idx).score;
            }
        }
    }
}


static void compute_score_way(type::PT_Data&, georef::GeoRef& georef) {
    //The scocre of each admin(level 8) is attributed to all its ways
    for (auto it = georef.fl_way.word_quality_list.begin(); it != georef.fl_way.word_quality_list.end(); ++it){
        for (navitia::georef::Admin* admin : georef.ways[it->first]->admin_list){
            if (admin->level == 8){
                it->second.score = georef.fl_admin.word_quality_list.at(admin->idx).score;
            }
        }
    }
}


static void compute_score_stop_point(type::PT_Data& pt_data, georef::GeoRef& georef) {
    //The scocre of each admin(level 8) is attributed to all its stop_points
    for (auto it = pt_data.stop_point_autocomplete.word_quality_list.begin(); it != pt_data.stop_point_autocomplete.word_quality_list.end(); ++it){
        for(navitia::georef::Admin* admin : pt_data.stop_points[it->first]->admin_list){
            if (admin->level == 8){
                it->second.score = georef.fl_admin.word_quality_list.at(admin->idx).score;
            }
        }
    }
}

static size_t admin_score(const std::vector<navitia::georef::Admin*>& admins, const georef::GeoRef &georef) {
    for (const auto* admin: admins) {
        if (admin->level == 8) return georef.fl_admin.word_quality_list.at(admin->idx).score;
    }
    return 0;
}

static void compute_score_stop_area(type::PT_Data& pt_data, const georef::GeoRef& georef){
    //The scocre of each admin(level 8) is attributed to all its stop_areas also
    //Find the stop-point count in all stop_areas and keep the highest;
    size_t max_score = 0;

    for (navitia::type::StopArea* sa : pt_data.stop_areas){
        max_score = std::max(max_score, sa->stop_point_list.size());
    }

    //Ajust the score of each stop_area from 0 to 100 using maximum score (max_score)
    if (max_score > 0){
        for (auto & it : pt_data.stop_area_autocomplete.word_quality_list){
            const size_t ad_score = admin_score(pt_data.stop_areas[it.first]->admin_list, georef);
            it.second.score = ad_score + (pt_data.stop_areas[it.first]->stop_point_list.size() * 100)/max_score;
        }
    }
}

/*
 Use natural logarithm to compute admin score as explained below.
 City       stop_point_count    log(n+2)*10
 Paris      3065                80
 Lyon       1000                69
 Versailles 418                 60
 St Denis   248                 55
 Melun      64                  42
 Pouzioux   1                   11
 Pampa      0                   7
*/
static void compute_score_admin(type::PT_Data& pt_data, georef::GeoRef& georef) {
    //For each stop_point increase the score of it's admin(level 8) by 1.
    for (navitia::georef::Way* way: georef.ways) {
        for (navitia::georef::Admin * admin : way->admin_list){
            if (admin->level == 8){
                georef.fl_admin.word_quality_list.at(admin->idx).score++;
            }
        }
    }
    std::set<navitia::georef::Admin*> without_way_admins;
    for(auto admin : georef.admins) {
        if (admin->level == 8 && georef.fl_admin.word_quality_list.at(admin->idx).score == 0) {
            without_way_admins.insert(admin);
        }
    }
    for (navitia::type::StopPoint* sp : pt_data.stop_points){
        for (navitia::georef::Admin * admin : sp->admin_list){
            if (admin->level == 8 && without_way_admins.count(admin) > 0){
                georef.fl_admin.word_quality_list.at(admin->idx).score++;
            }
        }
    }

    //Ajust the score of each admin using natural logarithm as : log(n+2)*10
    for (auto it = georef.fl_admin.word_quality_list.begin(); it != georef.fl_admin.word_quality_list.end(); ++it){
        it->second.score = log(it->second.score + 2) * 10;
    }
}

template<typename T>
void Autocomplete<T>::compute_score(type::PT_Data &pt_data, georef::GeoRef &georef,
                   const type::Type_e type) {
    switch(type){
        case type::Type_e::StopArea:
            compute_score_stop_area(pt_data, georef);
            break;
        case type::Type_e::StopPoint:
            compute_score_stop_point(pt_data, georef);
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

template<class T>
std::vector<typename Autocomplete<T>::fl_quality>
Autocomplete<T>::compute_vec_quality(const std::string& str,
                                     const std::vector<T>& index_result,
                                     const navitia::georef::GeoRef& geo_ref,
                                     std::function<bool(T)> keep_element,
                                     int wordLength) const {
    std::vector<fl_quality> vec_quality;
    std::vector<std::string> tokens_req;
    {
        auto tmp_str = strip_accents_and_lower(str);
        boost::split(tokens_req, tmp_str, boost::is_any_of(" "));
    }
    for (auto i : index_result) {
        if (keep_element(i)) {
            fl_quality quality;

            quality.idx = i;
            quality.nb_found = word_quality_list.at(quality.idx).word_count;
            quality.word_len = wordLength;
            quality.score = word_quality_list.at(quality.idx).score;
            quality.quality = 100;

            // whole_way_name = road name + "(" +  city name + ")"
            auto whole_way_name = geo_ref.ways[i]->name;
            for (const auto* admin: geo_ref.ways[i]->admin_list) {
                if (admin && admin->level == 8) {
                    whole_way_name += (" " + admin->name);
                }
            }
            whole_way_name = strip_accents_and_lower(whole_way_name);
            boost::algorithm::replace_all(whole_way_name, "-", " ");

            boost::tokenizer<> tokens_candidate(whole_way_name);
            auto it_req = std::begin(tokens_req);
            auto it_can = std::begin(tokens_candidate);
            for (; it_req != std::end(tokens_req) && it_can != std::end(tokens_candidate); ++it_req, ++it_can ) {
                quality.score += (*it_can).find(*it_req) != std::string::npos;
            }
            vec_quality.emplace_back(std::move(quality));
        }
    }
    return vec_quality;
}

template<class T>
std::vector<typename Autocomplete<T>::fl_quality>
Autocomplete<T>::find_complete_way(const std::string& str,
                                   size_t nbmax,
                                   std::function<bool(T)> keep_element,
                                   const std::set<std::string>& ghostwords,
                                   const navitia::georef::GeoRef& geo_ref) const{
    auto vec = tokenize(str, ghostwords);
    //Vector des ObjetTC index trouvés
    auto index_result = find(vec);
    // Créer un vector de réponse:
    auto vec_quality = compute_vec_quality(str, index_result, geo_ref, keep_element, words_length(vec));
    sort_and_truncate_by_score(vec_quality, nbmax);
    return vec_quality;
}

// https://isocpp.org/wiki/faq/templates#separate-template-class-defn-from-decl
// http://stackoverflow.com/a/32593884/1614576
template struct Autocomplete<nt::idx_t>;

}}
