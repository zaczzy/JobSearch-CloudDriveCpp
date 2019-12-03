#include <unordered_map>
#include <fstream>
#include  <iostream>
#include <boost/serialization/map.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/binary_oarchive.hpp> 
#include <boost/archive/binary_iarchive.hpp> 
#include <boost/serialization/unordered_map.hpp> 
#include <boost/serialization/string.hpp> 
#include <boost/serialization/list.hpp> 

typedef struct 
{
    int a;
    int b;
   std::unordered_map<int, std::string> second_map;
   
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & a;
       ar & b;
       ar & second_map;
   }
}test;

int main()
{
    test t1;
    t1.a = 5;
    t1.b = 10;
    t1.second_map.insert(std::make_pair(15, "ritika"));
    test t2;
    t2.a = 15;
    t2.b = 20;
    t2.second_map.insert(std::make_pair(15, "gupta"));
   std::unordered_map<int, test> my_map;

   int key1 = 1, key2 = 2;
   my_map.insert(std::make_pair(key1, t1));
   my_map.insert(std::make_pair(key2, t2));

   std::ofstream ofs("myfile.txt");
   boost::archive::text_oarchive oa(ofs);
   oa << my_map;
   ofs.close();

   std::unordered_map<int, test> new_map;
   std::ifstream ifs("myfile.txt");
   boost::archive::text_iarchive ia(ifs);
   ia >> new_map;

   for (std::unordered_map<int,test>::iterator it=new_map.begin(); it!=new_map.end(); ++it)
   {
        printf("t1.a = %d t1.b = %d int: %d\n", it->second.a, it->second.b, it->first);

        std::unordered_map<int, std::string>::iterator itr;
        for(itr = it->second.second_map.begin(); itr != it->second.second_map.end(); itr++)
            printf("second map: %d %s\n", itr->first, itr->second.c_str());
   }
}
