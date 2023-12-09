#include "CMSDK_CM3.h"
#include "core_cm3.h"

// simulate a hardware interrupt
void CallIRQN(IRQn_Type irqn, uint32_t Priority) {
    uint32_t priority = NVIC_EncodePriority(0, Priority, 0);
    NVIC_SetPriority(irqn, priority);
    NVIC_EnableIRQ(irqn);
    NVIC_SetPendingIRQ(irqn);
}
