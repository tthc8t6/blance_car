#ifndef TASK_H
#define TASK_H
#define PERIODIC(T)\
  static uint32_t nxt=0;\
  if(HAL_GetTick() < nxt) return;\
  nxt=HAL_GetTick()+T;

#endif // TASK_H
