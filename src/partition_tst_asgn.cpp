#include "partition_tst_asgn.hpp"

#include <algorithm>
#include <list>
#include "utils.hpp"


using namespace std;
using namespace sdf;


// bool Partition::operator==(const Partition &rhs) const { return repr == rhs.repr; }
// bool Partition::operator!=(const Partition &rhs) const { return !(rhs == *this); }

std::ostream& operator<<(ostream& out, const Partition& p)
{
    auto convert_v_to_str = [&p](V v)
            {
                auto convert_ec_to_str = [](const EC& ec){ return "{" + join(",", ec) + "}";};
                stringstream ss;
                ss << convert_ec_to_str(p.v_to_ec.at(v)) << " -> ";
                ss << "{ ";
                ss << join(", ", p.g.get_children(v), [&p, &convert_ec_to_str](auto v){return convert_ec_to_str(p.v_to_ec.at(v));});
                ss << " }";
                return ss.str();
            };

    out << "{ ";
    out << join(", ", p.g.get_vertices(), convert_v_to_str);
    out << " }";

    return out;
}

size_t calc_hash_for_graph(const Graph& g)
{
    return xor_hash(g.get_vertices(),
                    [&](const V& v) {return hash<V>()(v) ^ xor_hash(g.get_children(v), std::hash<V>());});
                                   // hash_of_the_vertex ^ hash_of_the_children_container
}

size_t Partition::calc_hash() const
{
    auto hash_of_v_to_ec = xor_hash(this->v_to_ec,
                                    [](const pair<V,EC>& e) { return hash<V>()(e.first)^xor_hash(e.second, std::hash<string>());});

    auto hash_of_g =  calc_hash_for_graph(g);

    return hash_of_v_to_ec ^ hash_of_g;
}

std::ostream& operator<<(ostream& out, const Asgn& asgn)
{
    if (asgn.asgn.empty())
        return out << "<keep>";

    out << "{ ";
    out << join(", ", asgn.asgn, [](auto p) { return p.first + "↦" + "{" + join(",", p.second) + "}"; });
    out << " }";

    return out;
}


//////// assumes all registers of c are non-prime
//////set<string> prime(const set<string>& c)
//////{
//////    set<string> primed;
//////    for (const auto& r : c)
//////        primed.insert(r+"'");
//////    return primed;
//////}
//////
//////Partition Algo::compute_next(const Partition& p, const Tst& tst, const Asgn& asgn)
//////{
//////    /* Example:
//////     * current = {r1=r2 < r3 < r4}
//////     *     tst = (i<o, i=r2, r2<o<r3)
//////     *    asgn = {o->{r2}, i->{r3}}
//////     *
//////     *    next = {r1=r3 < r2 < r4}
//////     *
//////     * The algorithm is:
//////     * 1. create the partition over R ∪ DataVars:
//////     *    r1=r2=i < o < r3 < r4
//////     * 2. add R' variables wrt. asgn:
//////     *    r1=r2=i=r1'=r3' < o=r2' < r3 < r4=r4'
//////     * 3. Project into R':
//////     *    r1'=r3' < r2' < r4'
//////     * 4. Un-prime:
//////     *    r1=r3 < r2 < r4
//////     */
//////
//////    Partition result(p);
//////
//////    cout << "starting partition: " << p << endl
//////         << "tst: " << tst << endl
//////         << "asgn: " << asgn << endl;
//////
//////    add_data(tst, result);
//////    cout << "p with data: " << result << endl;
//////
//////    add_primed(asgn, result);
//////    cout << "p with primed: " << result << endl;
//////
//////    project_on_primed(result);
//////    cout << "p projected on R': " << result << endl;
//////
//////    unprime(result);
//////    cout << "p unprimed: " << result << endl;
//////
//////    return result;
//////}
//////
//////void Algo::unprime(Partition& p)
//////{
//////    for (auto& c : p.repr)
//////    {
//////        set<string> c_old = c;
//////        c.clear();
//////        for (const auto& r : c_old)
//////        {
//////            MASSERT(r.at(0) == 'r' and r.at(r.size()-1) == '\'', r);
//////            c.insert(r.substr(0, r.size() - 1));  // removing ' in r'
//////        }
//////    }
//////}
//////
//////void Algo::project_on_primed(Partition& p)
//////{
//////    // modify p by projecting into R' (i.e., remove all others)
//////    for (auto c_it = p.repr.begin(); c_it != p.repr.end(); )
//////    {
//////        for (auto r_it = c_it->begin(); r_it != c_it->end(); )
//////            // remove non-primed variables and update the for-loop iterator r_it
//////            if (r_it->at(r_it->size()-1) != '\'')
//////                r_it = c_it->erase(r_it);
//////            else
//////                r_it++;
//////        // remove the class if empty, and update the for-loop iterator c_it
//////        if (c_it->empty())
//////            c_it = p.repr.erase(c_it);
//////        else
//////            c_it++;
//////    }
//////}
//////
//////void Algo::add_primed(const Asgn& asgn, Partition& p_with_data)
//////{
//////    // update p with primed registers according to asgn
//////
//////    // 1. add to the classes of d also their d->{r1,r2} (but prime them)
//////    set<string> assignedR;
//////    for (const auto& d_ac : asgn.asgn)
//////    {
//////        auto d = d_ac.first;
//////        auto asgnC = d_ac.second;
//////        auto primedAsgnC = prime(asgnC);
//////        p_with_data.get_class_of(d).insert(primedAsgnC.begin(), primedAsgnC.end());
//////
//////        assignedR.insert(asgnC.begin(), asgnC.end());  // (will be used in future)
//////    }
//////
//////    // 2: for unchanged r, add r' to the class of r
//////    set<string> unchanged = a_minus_b(p_with_data.getR(), assignedR);
//////    for (const auto& r : unchanged)
//////        p_with_data.get_class_of(r).insert(r + "'");
//////}
//////
///////* create the partition over R ∪ DataVars */
//////void Algo::add_data(const Tst& tst, Partition& p)
//////{
//////    for (const set<string>& d_class : tst.data_partition.repr)  // e.g., iterating over [{i}, {o}]
//////    {
//////        string d = *d_class.begin();  // take _any_ d from the equivalence class {i} (if we had tst = (i=o, ...), the we could arbitrary choose i or o)
//////        Cmp d_cmp = tst.data_to_cmp.at(d);  // note that all elements from the same class have the same cmp.
//////        if (d_cmp.is_EQUAL())  // ... of the form *=r2
//////        {
//////            p.get_class_of(d_cmp.rL).insert(d_class.begin(), d_class.end());  // insert all d into the same class
//////        }
//////        else if (d_cmp.is_BELOW())
//////        {
//////            p.repr.push_front(d_class);
//////        }
//////        else if (d_cmp.is_ABOVE())
//////        {
//////            p.repr.push_back(d_class);
//////        }
//////        else if (d_cmp.is_BETWEEN())  // .. the form  r1<*<r2
//////        {
//////            {   // debug
//////                auto pos1 = find(p.repr.begin(), p.repr.end(), p.get_class_of(d_cmp.rL));
//////                auto pos2 = find(p.repr.begin(), p.repr.end(), p.get_class_of(d_cmp.rU));
//////                auto dist = distance(pos1, pos2);
//////                MASSERT(dist>0, "for cmp rL<*<rU, rL must be smaller than rU: " << p);
//////            }
//////            // we need to find the right place in p_with_data: we insert right before rU.
//////            // This is correct because we traverse d0<d1<d2<d3 from smaller di-1 to larger di,
//////            // and all previous di-1 were already inserted.
//////            auto pos = find(p.repr.begin(), p.repr.end(), p.get_class_of(d_cmp.rU));
//////
//////            p.repr.insert(pos, d_class);   // `list::insert` inserts _before_ pos
//////        }
//////        else
//////            MASSERT(0, "unknown type of cmp: " << d_cmp);
//////    }
//////}

