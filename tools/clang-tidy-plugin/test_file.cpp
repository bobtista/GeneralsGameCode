// Test file for the UseIsEmptyCheck
#include "Common/AsciiString.h"
#include "Common/UnicodeString.h"

void testAsciiString(AsciiString str) {
    // Should be flagged: getLength() == 0
    if (str.getLength() == 0) {
        // do something
    }
    
    // Should be flagged: getLength() > 0
    if (str.getLength() > 0) {
        // do something
    }
    
    // Should be flagged: getLength() != 0
    if (str.getLength() != 0) {
        // do something
    }
    
    // Should be flagged: getLength() <= 0
    if (str.getLength() <= 0) {
        // do something
    }
    
    // Should NOT be flagged: isEmpty() is already correct
    if (str.isEmpty()) {
        // do something
    }
}

void testUnicodeString(UnicodeString str) {
    // Should be flagged: getLength() == 0
    if (str.getLength() == 0) {
        // do something
    }
    
    // Should be flagged: getLength() > 0
    if (str.getLength() > 0) {
        // do something
    }
}

