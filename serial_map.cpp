#include <unordered_map>
#include <sstream>
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
   
    friend class boost::serialization::access;
   template<class Archive>
   void serialize(Archive & ar, const unsigned int version)
   {
       // Simply list all the fields to be serialized/deserialized.
       ar & a;
       ar & b;
   }
}test;

int main()
{
    test t1;
    t1.a = 5;
    t1.b = 10;
    test t2;
    t2.a = 15;
    t2.b = 20;
   std::unordered_map<int, test> my_map;

   int key1 = 1, key2 = 2;
   my_map.insert(std::make_pair(key1, t1));
   my_map.insert(std::make_pair(key2, t2));

   //serializable_class my_class;
   //my_class.my_map = my_map;
   std::stringstream ss;
   boost::archive::text_oarchive oarch(ss);
   oarch << my_map;
   std::unordered_map<int, test> new_map;
   boost::archive::text_iarchive iarch(ss);
   iarch >> new_map;
   //std::cout << (my_map == new_map) << std::endl;

   for (std::unordered_map<int,test>::iterator it=new_map.begin(); it!=new_map.end(); ++it)
        printf("t1.a = %d t1.b = %d int: %d\n", it->second.a, it->second.b, it->first);
}
