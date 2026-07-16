#ifndef __TASK_H
#define __TASK_H
#define PERIODIC(T)\
  static uint32_t nxt=0;\
  if(HAL_GetTick() < nxt) return;\
  nxt=HAL_GetTick()+T;

#endif // __TASK_H
