#ifndef SINDARIN_DEBUG_H
#define SINDARIN_DEBUG_H

#define DEBUG_VAR(x) if (Serial) Serial.print(x)
#define DEBUG_LN(y) if (Serial) Serial.println(y)
#define DEBUG_F(vargs...) \
    if (Serial)           \
    Serial.printf(vargs)

#endif
