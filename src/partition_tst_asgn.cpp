#include "partition_tst_asgn.hpp"

#include <algorithm>
#include <list>
#include "utils.hpp"


using namespace std;
using namespace sdf;


string Cmp::to_str() const
{
    return r2.empty()? "*="+r1 : r1+"<*<"+r2;  // must use '*' (Tst::to_str relies on this)
}


string Tst::to_str() const
{
    vector<string> values;
    transform(data_to_cmp.begin(), data_to_cmp.end(),
              back_inserter(values),
              [](const auto& e){return substitute(e.second.to_str(), "*", e.first);});
    return "(" +
           data_partition.to_str() + ", " + join(", ", values) +
           ")";
}


std::string Partition::to_str() const
{
    // given [{"r1","r2"}, {"r3"}]
    // ["r1=r2", "r3"]
    // "r1=r2<r3"
    vector<string> aux;
    transform(repr.begin(), repr.end(),
              back_inserter(aux),
              [](const set<string>& c){return join("=",c);});
    return join("<", aux);
}


std::set<std::string>& Partition::get_class_of(const string& r)
{
    for (auto& c : this->repr)
        if (contains(c,r))
            return c;

    MASSERT(0, "unreachable");
}

set<string> Partition::getR() const
{
    set<string> R;
    for (const auto& c : repr)
        for (const auto& r : c)
            if (r.at(0) == 'r' and r.at(r.size()-1) != '\'')
                R.insert(r);
    return R;
}


string Asgn::to_str() const
{
    if (asgn.empty())
        return "<keep>";
    vector<string> arrows;
    transform(asgn.begin(), asgn.end(), back_inserter(arrows), [](auto p){return p.first + "↦" + "{"+join(",",p.second)+"}";});
    return "{" + join(", ", arrows) + "}";
}


// assumes all registers of c are non-prime
set<string> prime(const set<string>& c)
{
    set<string> primed;
    for (const auto& r : c)
        primed.insert(r+"'");
    return primed;
}

Partition Algo::compute_next(const Partition& p, const Tst& tst, const Asgn& asgn)
{
    /* Example:
     * current = {r1=r2 < r3 < r4}
     *     tst = (i<o, i=r2, r2<o<r3)
     *    asgn = {o->{r2}, i->{r3}}
     *
     *    next = {r1=r3 < r2 < r4}
     *
     * The algorithm is:
     * 1. create the partition over R ∪ DataVars:
     *    r1=r2=i < o < r3 < r4
     * 2. add R' variables wrt. asgn:
     *    r1=r2=i=r1'=r3' < o=r2' < r3 < r4=r4'
     * 3. Project into R':
     *    r1'=r3' < r2' < r4'
     * 4. Un-prime:
     *    r1=r3 < r2 < r4
     */

    Partition result(p);

    cout << "starting partition: " << p << endl
         << "tst: " << tst << endl
         << "asgn: " << asgn << endl;

    add_data(tst, result);
    cout << "p with data: " << result << endl;

    add_primed(asgn, result);
    cout << "p with primed: " << result << endl;

    project_on_primed(result);
    cout << "p projected on R': " << result << endl;

    unprime(result);
    cout << "p unprimed: " << result << endl;

    return result;
}

void Algo::unprime(Partition& p)
{
    for (auto& c : p.repr)
    {
        set<string> c_old = c;
        c.clear();
        for (const auto& r : c_old)
        {
            MASSERT(r.at(0) == 'r' and r.at(r.size()-1) == '\'', r);
            c.insert(r.substr(0, r.size() - 1));  // removing ' in r'
        }
    }
}

void Algo::project_on_primed(Partition& p)
{
    // modify p by projecting into R' (i.e., remove all others)
    for (auto c_it = p.repr.begin(); c_it != p.repr.end(); )
    {
        for (auto r_it = c_it->begin(); r_it != c_it->end(); )
            // remove non-primed variables and update the for-loop iterator r_it
            if (r_it->at(r_it->size()-1) != '\'')
                r_it = c_it->erase(r_it);
            else
                r_it++;
        // remove the class if empty, and update the for-loop iterator c_it
        if (c_it->empty())
            c_it = p.repr.erase(c_it);
        else
            c_it++;
    }
}

void Algo::add_primed(const Asgn& asgn, Partition& p_with_data)
{
    // update p with primed registers according to asgn

    // 1. add to the classes of d also their d->{r1,r2} (but prime them)
    set<string> assignedR;
    for (const auto& d_ac : asgn.asgn)
    {
        auto d = d_ac.first;
        auto asgnC = d_ac.second;
        auto primedAsgnC = prime(asgnC);
        p_with_data.get_class_of(d).insert(primedAsgnC.begin(), primedAsgnC.end());

        assignedR.insert(asgnC.begin(), asgnC.end());  // (will be used in future)
    }

    // 2: for unchanged r, add r' to the class of r
    set<string> unchanged = a_minus_b(p_with_data.getR(), assignedR);
    for (const auto& r : unchanged)
        p_with_data.get_class_of(r).insert(r + "'");
}

/* create the partition over R ∪ DataVars */
void Algo::add_data(const Tst& tst, Partition& p)
{
    for (const set<string>& d_class : tst.data_partition.repr)  // e.g., iterating over [{i}, {o}]
    {
        string d = *d_class.begin();  // take _any_ d from the equivalence class {i} (if we had tst = (i=o, ...), the we could arbitrary choose i or o)
        Cmp d_cmp = tst.data_to_cmp.at(d);  // note that all elements from the same class have the same cmp.
        if (d_cmp.type == Cmp::EQUAL)  // ... of the form *=r2
        {
            p.get_class_of(d_cmp.r1).insert(d_class.begin(), d_class.end());  // insert all d into the same class
        }
        else if (d_cmp.type == Cmp::BELOW)
        {
            p.repr.push_front(d_class);
        }
        else if (d_cmp.type == Cmp::ABOVE)
        {
            p.repr.push_back(d_class);
        }
        else if (d_cmp.type == Cmp::BETWEEN)  // .. the form  r1<*<r2
        {
            {   // debug
                auto pos1 = find(p.repr.begin(), p.repr.end(), p.get_class_of(d_cmp.r1));
                auto pos2 = find(p.repr.begin(), p.repr.end(), p.get_class_of(d_cmp.r2));
                auto dist = distance(pos1, pos2);
                MASSERT(dist>0, "for cmp r1<*<r2, r1 must be smaller than r2: " << p);
            }
            // we need to find the right place in p_with_data: we insert right before r2.
            // This is correct because we traverse d0<d1<d2<d3 from smaller di-1 to larger di,
            // and all previous di-1 were already inserted.
            auto pos = find(p.repr.begin(), p.repr.end(), p.get_class_of(d_cmp.r2));

            p.repr.insert(pos, d_class);   // `list::insert` inserts _before_ pos
        }
        else
            MASSERT(0, "unknown type of cmp: " << d_cmp);
    }
}













