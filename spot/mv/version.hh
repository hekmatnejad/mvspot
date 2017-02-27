/*
Version information of the library
by: Mohammad Hekmatnejad
*/
#ifndef MVVERSION_H
#define MVVERSION_H
#include <iostream>
using namespace std;

namespace mvspot
{
    const string current_version = "Multi Valued SPOT Version: 0.01";
    const string current_build = "02/24/2017-19:07";
    const string getVersion(){
        return current_version;
    }
    const string getBuild(){
        return current_build;
    }
}

#endif