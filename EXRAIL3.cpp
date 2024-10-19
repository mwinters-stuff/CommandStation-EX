// file exists to break #include dependency loop
#include "EXRAIL2.h"
#include "EXRAIL3.h"
void RMFT3::blockEvent(int16_t block, int16_t loco, bool entering)  {
    RMFT2::blockEvent(block,loco,entering);
};
