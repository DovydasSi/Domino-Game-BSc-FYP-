/******************************************************************************

                              Online C++ Compiler.
               Code, Compile, Run and Debug C++ program online.
Write your code in this editor and press "Run" button to compile and execute it.

*******************************************************************************/

#include <iostream>

using namespace std;

#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <set>

template<typename T>
struct equivalence_class {
    std::set<std::pair<T,T>> equivalence_relationship;
    std::set<T> items;
    std::unordered_map<T, std::unordered_set<T>> class_id_to_members;

    equivalence_class() : hasTransition{false} {};

    void insert(const T& left, const T& right) {
        equivalence_relationship.emplace(left, right); //
        equivalence_relationship.emplace(right, left); // Symmetry
        equivalence_relationship.emplace(right, right); // Identity
        equivalence_relationship.emplace(left, left); // Identity
        items.insert(left);
        items.insert(right);
        hasTransition = false;
        class_id_to_members.clear();
    }

    std::unordered_map<T, std::unordered_set<T>> calculateEquivalenceClass() {
        if (!hasTransition) {
            // Transitive
            for (const auto& k : items) {
                for (const auto& i : items) {
                    for (const auto& j : items) {
                        if ((equivalence_relationship.find(std::make_pair(i, j)) != equivalence_relationship.end()) || ((equivalence_relationship.find(std::make_pair(i, k)) != equivalence_relationship.end()) && (equivalence_relationship.find(std::make_pair(k, j))) != equivalence_relationship.end()))
                            equivalence_relationship.emplace(i, j);
                    }
                }
            }
            hasTransition = true;

            std::unordered_map<T, T> non_class_ref_element_to_class_id;

            for (auto it = equivalence_relationship.begin(); it != equivalence_relationship.end(); ) {
                if ((non_class_ref_element_to_class_id.find(it->second)  == non_class_ref_element_to_class_id.end()) &&
                    (((items.find(it->first) != items.end()) || (class_id_to_members.find(it->first) !=  class_id_to_members.end())))) {
                    items.erase(it->first);
                    items.erase(it->second);
                    class_id_to_members[it->first].emplace(it->second);
                    if (it->first != it->second) {
                        non_class_ref_element_to_class_id[it->second] = it->first;
                    }
                }
                it = equivalence_relationship.erase(it);
            }
        }
        return class_id_to_members;
    }

private:
    bool hasTransition = false;

};


/*

int main()
{

    equivalence_class<int> eq;
    eq.insert(1,2);
    eq.insert(3,2);
    eq.insert(4,6);
    eq.insert(4,1);
    for (const auto& cp :
    eq.calculateEquivalenceClass()) {
        std::cout << cp.first << " = Candidate of the class" << std::endl;
        std::cout << "{" ;
        for (const auto& x : cp.second) {
            std::cout << x << ", ";
        }
        std::cout << "}" << std::endl;
    }
    cout<<"Hello World";

    return 0;
}
*/