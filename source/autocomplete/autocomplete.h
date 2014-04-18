#pragma once
#include <boost/tokenizer.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/map.hpp>
#include <algorithm>
#include <regex>
#include <boost/regex.hpp>


#include <map>
#include <unordered_map>
#include <set>
#include "type/type.h"

namespace navitia { namespace autocomplete {

struct Comapre {
    bool operator()(const  std::string& str_a, const std::string& str_b) const {
        if(str_a.length() == str_b.length()){
            return str_a >= str_b;
        }else{
            return (str_a.length() > str_b.length());
        }
    }
};

using autocomplete_map = std::map<std::string, std::string, Comapre>;
/** Map de type Autocomplete
  *
  * On associe une chaine de caractères, par exemple "rue jean jaures" à une valeur T (typiquement un pointeur
  * ou un indexe)
  *
  * Quand on cherche "r jean" on veut récupérer la valeur correspondant à la clef "rue jean jaures" et "rue jeanne d'arc"
  */
template<class T>
struct Autocomplete
{
    /// structure qui contient la position des mots dans autocomplete et le nombre de match.
    struct fl_quality{
        T idx;
        int nb_found;
        int word_len;
        int quality;
        navitia::type::GeographicalCoord coord;
        int house_number;

        fl_quality() :idx(0), nb_found(0), word_len(0), quality(0), house_number(-1) {}
        bool operator<(const fl_quality & other) const{
            return this->quality > other.quality;
        }

    };

    /// Structure temporaire pour garder les informations sur chaque ObjetTC:
    struct word_quality{
        int word_count;
        int word_distance;
        int score;

        word_quality():word_count(0), word_distance(0), score(0){}
        template<class Archive> void serialize(Archive & ar, const unsigned int) {
            ar & word_count & word_distance & score;
        }
    };

    /// Structure temporaire pour construire l'indexe
    std::map<std::string, std::set<T> > temp_word_map;

    /// À chaque mot (par exemple "rue" ou "jaures") on associe un tableau de T qui contient la liste des éléments contenant ce mot
    typedef std::pair<std::string, std::vector<T> > vec_elt;

    /// Structure principale de notre indexe
    std::vector<vec_elt> word_dictionnary;

    /// Structure temporaire pour garder les patterns et leurs indexs
    std::map<std::string, std::set<T> > temp_pattern_map;
    std::vector<vec_elt> pattern_dictionnary;

    /// Structure pour garder les informations comme nombre des mots, la distance des mots...dans chaque Autocomplete (Position)
    std::map<T, word_quality> word_quality_list;

    template<class Archive> void serialize(Archive & ar, const unsigned int) {
        ar & word_dictionnary & word_quality_list &pattern_dictionnary;
    }

    /// Efface les structures de données sérialisées
    void clear() {
        temp_word_map.clear();
        word_dictionnary.clear();
        temp_pattern_map.clear();
        pattern_dictionnary.clear();
        word_quality_list.clear();
    }

    // Méthodes permettant de construire l'indexe

    /** Étant donné une chaîne de caractères et la position de l'élément qui nous intéresse :
      * – on découpe en mots la chaîne (tokens)
      * — on rajoute la position à la liste de chaque mot
      */
    void add_string(std::string str, T position,
                    const autocomplete_map& synonyms){
        word_quality wc;
        int distance = 0;

        //Appeler la méthode pour traiter les synonymes avant de les ajouter dans le dictionaire:
        std::vector<std::string> vec_word = tokenize(str, synonyms);

        //créer des patterns pour chaque mot et les ajouter dans temp_pattern_map:
        add_vec_pattern(vec_word, position);

        int count = vec_word.size();
        auto vec = vec_word.begin();
        while(vec != vec_word.end()){
            temp_word_map[*vec].insert(position);
            distance += (*vec).size();
            ++vec;
        }
        wc.word_count = count;
        wc.word_distance = distance;
        wc.score = 0;
        word_quality_list[position] = wc;
    }

    void add_vec_pattern(const std::vector<std::string> &vec_words, T position){
        //Créer les patterns:
        std::vector<std::string> vec_patt = make_vec_pattern(vec_words, 2);
        auto v_patt = vec_patt.begin();
        while(v_patt != vec_patt.end()){
            temp_pattern_map[*v_patt].insert(position);
            ++v_patt;
        }
    }

    std::vector<std::string> make_vec_pattern(const std::vector<std::string> &vec_words, size_t n_gram) const{
        std::vector<std::string> pattern;
        auto vec = vec_words.begin();
        while(vec != vec_words.end()){
            std::string word = *vec;
            if (word.length() > n_gram){
                for (size_t i = 0;i <=  word.length() - n_gram; ++i){
                    pattern.push_back(word.substr(i,n_gram));
                }
            }
            else{
                pattern.push_back(word);
            }
            ++vec;
        }
        return pattern;
    }



    /** Construit la structure finale
      *
      * Les map et les set sont bien pratiques, mais leurs performances sont mauvaises avec des petites données (comme des ints)
      */
    void build(){
        word_dictionnary.reserve(temp_word_map.size());
        for(auto key_val: temp_word_map){
            word_dictionnary.push_back(std::make_pair(key_val.first, std::vector<T>(key_val.second.begin(), key_val.second.end())));
        }

        //Dictionnaire des patterns:
        pattern_dictionnary.reserve((temp_pattern_map.size()));
        for(auto key_val:temp_pattern_map){
            pattern_dictionnary.push_back(std::make_pair(key_val.first, std::vector<T>(key_val.second.begin(), key_val.second.end())));
        }
    }

    //Méthode pour calculer le score de chaque élément par son admin.
    void compute_score(type::PT_Data &pt_data, georef::GeoRef &georef,
                       const type::Type_e type);
    // Méthodes premettant de retrouver nos éléments
    /** Définit un fonctor permettant de parcourir notre structure un peu particulière */
    struct comp{
        /** Utilisé pour trouver la borne inf. Quand on cherche av, on veux que avenue soit également trouvé
          * Il faut donc que "av" < "avenue" soit false
          */
        bool operator()(const std::string & a, const std::pair<std::string, std::vector<T> > & b){
            if(b.first.find(a) == 0) return false;
            return (a < b.first);
        }

        /** Utilisé pour la borne sup. Ici rien d'extraordinaire */
        bool operator()(const std::pair<std::string, std::vector<T> > & b, const std::string & a){
            return (b.first < a);
        }
    };

    /** Retrouve toutes les positions des élements contenant le mot des mots qui commencent par token */
    std::vector<T> match(const std::string &token, const std::vector<vec_elt> &vec_source) const {
        // Les éléments dans vec_map sont triés par ordre alphabétiques, il suffit donc de trouver la borne inf et sup
        auto lower = std::lower_bound(vec_source.begin(), vec_source.end(), token, comp());
        auto upper = std::upper_bound(vec_source.begin(), vec_source.end(), token, comp());

        std::vector<T> result;

        // On concatène tous les indexes
        // Pour les raisons de perfs mesurées expérimentalement, on accepte des doublons
        for(; lower != upper; ++lower){
            std::vector<T> other = lower->second;
            result.insert(result.end(), other.begin(), other.end());
        }
        return result;
    }

    /** On passe une chaîne de charactère contenant des mots et on trouve toutes les positions contenant tous ces mots*/
    std::vector<T> find(std::vector<std::string> vecStr) const {
        std::vector<T> result;
        auto vec = vecStr.begin();
        if(vec != vecStr.end()){
            // Premier résultat. Il y aura au plus ces indexes
            result = match(*vec, word_dictionnary);

            for(++vec; vec != vecStr.end(); ++vec){
                std::vector<T> new_result;
                std::sort(result.begin(), result.end());
                for(auto i : match(*vec, word_dictionnary)){
                    // Binary search fait une recherche dichotomique pour savoir si l'élément i existe
                    // S'il existe dans les deux cas, on le garde
                    if(binary_search(result.begin(), result.end(), i)){
                        new_result.push_back(i);
                    }
                }
                //The function "unique" works only if the vector new_result is sorted.
                std::sort(new_result.begin(), new_result.end());

                // std::unique retrie les donnée et fout les doublons à la fin
                // La fonction retourne un itérateur vers le premier doublon
                // Expérimentalement le gain est très faible
                result.assign(new_result.begin(), std::unique(new_result.begin(), new_result.end()));
            }
        }
        return result;
    }

    /** Définit un fonctor permettant de parcourir notqualityre structure un peu particulière : trier par la valeur "nb_found"*/
    /** associé au valeur du vector<T> */
    struct Compare{
        const std::unordered_map<T, fl_quality>  & fl_result;// Pointeur au fl_result
        Compare(const std::unordered_map<T, fl_quality>  & fl_result) : fl_result(fl_result) {} //initialisation

        bool operator()(T a, T b) const{
            return fl_result.at(a).nb_found > fl_result.at(b).nb_found;
        }
    };

    /** Définit un fonctor permettant de parcourir notre structure un peu particulière : trier par la valeur "nb_found"*/
    /** associé au valeur du vector<T> */
    struct compare_by_quality{
        const std::unordered_map<T, fl_quality> & fl_result;
        compare_by_quality(const std::unordered_map<T, fl_quality>  & fl_result) : fl_result(fl_result) {} //initialisation

        bool operator()(T a, T b) const{
            return fl_result.at(a).quality > fl_result.at(b).quality;
        }
    };

    std::vector<fl_quality> sort_and_truncate(std::vector<fl_quality> input, size_t nbmax) const {
        typename std::vector<fl_quality>::iterator middle_iterator;
        if(nbmax < input.size())
            middle_iterator = input.begin() + nbmax;
        else
            middle_iterator = input.end();
        std::partial_sort(input.begin(), middle_iterator, input.end());

        if (input.size() > nbmax){input.resize(nbmax);}
        return input;
    }

    /** On passe une chaîne de charactère contenant des mots et on trouve toutes les positions contenant au moins un des mots*/
    std::vector<fl_quality> find_complete(const std::string & str,
                                          const autocomplete_map& synonyms,
                                          const int wordweight,
                                          size_t nbmax,
                                          std::function<bool(T)> keep_element)
                                          const{
        std::vector<std::string> vec = tokenize(str, synonyms);
        int wordCount = 0;
        int wordLength = 0;
        fl_quality quality;
        std::vector<T> index_result;
        //Vector des ObjetTC index trouvés
        index_result = find(vec);
        wordCount = vec.size();
        wordLength = words_length(vec);

        //Récupérer le Max score parmi les élément trouvé
        int max_score = 0;
        for (auto ir : index_result){
            if (keep_element(ir)){
                max_score = word_quality_list.at(ir).score > max_score ? word_quality_list.at(ir).score : max_score;
            }
        }

        // Créer un vector de réponse:
        std::vector<fl_quality> vec_quality;

        for(auto i : index_result){
            if(keep_element(i)) {
                quality.idx = i;
                quality.nb_found = wordCount;
                quality.word_len = wordLength;
                quality.quality = calc_quality_fl(quality, wordweight, max_score);
                vec_quality.push_back(quality);
            }
        }
        return sort_and_truncate(vec_quality, nbmax);
    }


    /** Recherche des patterns les plus proche : faute de frappe */
    std::vector<fl_quality> find_partial_with_pattern(const std::string &str,
                                                      const autocomplete_map& synonyms, const int word_weight,
                                                      size_t nbmax,
                                                      std::function<bool(T)> keep_element)
                                                      const{
        //Map temporaire pour garder les patterns trouvé:
        std::unordered_map<T, fl_quality> fl_result;

        //Vector temporaire des indexs
        std::vector<T> index_result;

        //Créer un vector de réponse
        std::vector<fl_quality> vec_quality;
        fl_quality quality;

        std::vector<std::string> vec_word = tokenize(str, synonyms);
        std::vector<std::string> vec_pattern = make_vec_pattern(vec_word, 2); //2-grams
        int wordLength = words_length(vec_word);
        int pattern_count = vec_pattern.size();

        //recherche pour le premier pattern:
        auto vec = vec_pattern.begin();
        if (vec != vec_pattern.end()){
            //Premier résultat:
            index_result = match(*vec, pattern_dictionnary);

            //Incrémenter la propriété "nb_found" pour chaque index des mots autocomplete dans vec_map
            add_word_quality(fl_result,index_result);

            //Recherche des mots qui restent
            for (++vec; vec != vec_pattern.end(); ++vec){
                index_result = match(*vec, pattern_dictionnary);

                //incrémenter la propriété "nb_found" pour chaque index des mots autocomplete dans vec_map
                add_word_quality(fl_result,index_result);
            }

            //Récupérer le Max score parmi les élément trouvé
            int max_score = 0;
            for (auto ir : index_result){
                if (keep_element(ir)){
                    max_score = word_quality_list.at(ir).score > max_score ? word_quality_list.at(ir).score : max_score;
                }
            }

            //remplir le tableau temp_result avec le résultat de qualité.
            //A ne pas remonter l'objectTC si le  faute de frappe est > 2
            for(auto pair : fl_result){
                //if (keep_element(pair.first) && (pair.second.nb_found > pattern_count - 3)){
                if (keep_element(pair.first) && (((pattern_count - pair.second.nb_found) * 100) / pattern_count <= 25)){
                    quality.idx = pair.first;
                    quality.nb_found = pair.second.nb_found;
                    quality.word_len = wordLength;
                    quality.quality = calc_quality_pattern(quality, word_weight, max_score, pattern_count);
                    vec_quality.push_back(quality);
                }
            }
        }
        return sort_and_truncate(vec_quality, nbmax);
    }


    /** pour chaque mot trouvé dans la liste des mots il faut incrémenter la propriété : nb_found*/
    /** Utilisé que pour une recherche partielle */
    void add_word_quality(std::unordered_map<T, fl_quality> & fl_result, const std::vector<T> &found) const{
        for(auto i : found){
            fl_result[i].nb_found++;
        }
    }

    int calc_quality_fl(const fl_quality & ql,  int wordweight, int max_score) const {
        int result = 100;

        //Qualité sur le nombres des mot trouvé
        result -= (word_quality_list.at(ql.idx).word_count - ql.nb_found) * wordweight;//coeff  WordFound

        //Qualité sur la distance globale des mots.
        result -= (word_quality_list.at(ql.idx).word_distance - ql.word_len);//Coeff de la distance = 1

        //Qualité sur le score
        result -= (max_score - word_quality_list.at(ql.idx).score)/10;
        return result;
    }

    int calc_quality_pattern(const fl_quality & ql,  int wordweight, int max_score, int patt_count) const {
        int result = 100;

        //Qualité sur le nombres des mot trouvé
        result -= (patt_count - ql.nb_found) * wordweight;//coeff  WordFound

        //Qualité sur la distance globale des mots.
        result -= abs(word_quality_list.at(ql.idx).word_distance - ql.word_len);//Coeff de la distance = 1

        //Qualité sur le score
        result -= (max_score - word_quality_list.at(ql.idx).score)/10;
        return result;
    }

    int words_length(std::vector<std::string> & words) const{
        int distance = 0;
        auto vec = words.begin();
        while(vec != words.end()){
            distance += (*vec).size();
            ++vec;
        }
        return distance;
    }

    std::string strip_accents(std::string str) const {
        std::vector< std::pair<std::string, std::string> > vec_str;
        vec_str.push_back(std::make_pair("à","a"));
        vec_str.push_back(std::make_pair("À","a"));
        vec_str.push_back(std::make_pair("â","a"));
        vec_str.push_back(std::make_pair("Â","a"));
        vec_str.push_back(std::make_pair("æ","ae"));
        vec_str.push_back(std::make_pair("é","e"));
        vec_str.push_back(std::make_pair("É","e"));
        vec_str.push_back(std::make_pair("è","e"));
        vec_str.push_back(std::make_pair("È","e"));
        vec_str.push_back(std::make_pair("ê","e"));
        vec_str.push_back(std::make_pair("Ê","e"));
        vec_str.push_back(std::make_pair("ô","o"));
        vec_str.push_back(std::make_pair("Ô","o"));
        vec_str.push_back(std::make_pair("û","u"));
        vec_str.push_back(std::make_pair("Û","u"));
        vec_str.push_back(std::make_pair("ù","u"));
        vec_str.push_back(std::make_pair("Ù","u"));
        vec_str.push_back(std::make_pair("ç","c"));
        vec_str.push_back(std::make_pair("Ç","c"));
        vec_str.push_back(std::make_pair("ï","i"));
        vec_str.push_back(std::make_pair("Ï","i"));
        vec_str.push_back(std::make_pair("œ","oe"));

        auto vec = vec_str.begin();
        while(vec != vec_str.end()){
            boost::algorithm::replace_all(str, vec->first, vec->second);
            ++vec;
        }
        return str;
    }

    std::vector<std::string> tokenize(std::string strFind, const autocomplete_map& synonyms) const{
        std::vector<std::string> vec;
        boost::to_lower(strFind);
        strFind = boost::regex_replace(strFind, boost::regex("( ){2,}"), " ");

        //traiter les caractères accentués
        strFind = strip_accents(strFind);

       for(const auto& it : synonyms){
            strFind = boost::regex_replace(strFind,boost::regex("\\<" + it.first + "\\>"), it.second);
        }

        boost::tokenizer <> tokens(strFind);
        for (auto token_it: tokens){
            if (!token_it.empty()){
                vec.push_back(token_it);
            }
        }
       return vec;
    }

    bool is_address_type(const std::string & str,
                         const autocomplete_map& synonyms) const{
        bool result = false;
        std::vector<std::string> vec_token = tokenize(str, synonyms);
        std::vector<std::string> vecTpye = {"rue", "avenue", "place", "boulevard","chemin", "impasse"};
        auto vtok = vec_token.begin();
        while(vtok != vec_token.end() && (result == false)){
            //Comparer avec le vectorType:
            auto vtype = vecTpye.begin();
            while(vtype != vecTpye.end() && (result == false)){
                if (*vtok == *vtype){
                    result = true;

                }
                ++vtype;
            }
            ++vtok;
        }
        return result;
    }
};

}} // namespace navitia::autocomplete
