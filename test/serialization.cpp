//
// Created by swx on 23-6-4.
//


#include <fstream>

// include headers that implement a archive in simple text format
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <iostream>
#include <unordered_map>
#include "boost/serialization/serialization.hpp"
#include "boost/archive/binary_oarchive.hpp"
#include "boost/archive/binary_iarchive.hpp"
#include <boost/serialization/export.hpp>
#include "boost/foreach.hpp"
#include "boost/any.hpp"
#include <boost/serialization/vector.hpp>
#include <boost/serialization/unordered_map.hpp>

using namespace  std;
// gps coordinate
//
// illustrates serialization for a simple type
//
class gps_position
{
private:
    friend class boost::serialization::access;
    // When the class Archive corresponds to an output archive, the
    // & operator is defined similar to <<.  Likewise, when the class Archive
    // is a type of input archive the & operator is defined similar to >>.
    template<class Archive>
    void serialize(Archive & ar, const unsigned int version)
    {
        ar & degrees;
        ar & minutes;
        ar & seconds;
        ar & umap;
    }
    int degrees;
    int minutes;
    float seconds;
    unordered_map<string,int> umap;
public:
    void show(){
        cout<<"degrees"<<degrees<<endl;
        cout<<"minutes"<<minutes<<endl;
        cout<<"seconds"<<seconds<<endl;
        cout<<"umap";
        for(auto item:umap){
            cout<<item.first<<"  "<<item.second;
        }cout<<endl;
    }
    gps_position(){};
    gps_position(int d, int m, float s) :
            degrees(d), minutes(m), seconds(s)
    {
        umap.insert(make_pair("hello",1));
        umap.insert(make_pair("world",2));
    }
};

int main() {
    // create and open a character archive for output
    std::stringstream ss;
    boost::archive::text_oarchive oa(ss);

    // create class instance
    const gps_position g(35, 59, 24.567f);
    // write class instance to archive
    oa << g;
    // close archive
    std::string  res = ss.str();
    ss.clear();

    // ... some time later restore the class instance to its orginal state
    // create and open an archive for input
    std::stringstream iss(res);
    boost::archive::text_iarchive ia(iss);
    // read class state from archive
    gps_position newg;
    newg.show();
    ia >> newg;
    newg.show();
    // close archive
    iss.clear();
    return 0;

}

//
