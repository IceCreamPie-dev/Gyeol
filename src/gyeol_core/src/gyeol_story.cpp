#include "gyeol_story.h"
#include "gyeol_generated.h" 
#include <iostream>

using namespace ICPDev::Gyeol::Schema;

namespace Gyeol {
    void Story::printVersion() {
        std::cout << "Gyeol Engine Core Initialized." << std::endl;
        std::cout << "FlatBuffers Schema Loaded." << std::endl;
    }
}
