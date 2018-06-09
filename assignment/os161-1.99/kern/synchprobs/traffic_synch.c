#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>
#include <array.h>
/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */
/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */
/*
 * replace this with declarations of any synchronization and other variables you need here
 */
typedef struct Vehicles 
{ 	
  Direction origin; 	
  Direction destination; 
} Vehicle;
struct lock *traffic_lock;
// struct cv *turnLeft;
struct cv *go;
struct array *vehicles;
//bool go_straight(Direction origin, Direction destination);
//bool turn_left(Direction origin, Direction destination);
bool turn_right(Direction origin, Direction destination);
bool check_invalid_condition(Direction origin, Direction destination);
//bool check_invalid_conditionB(Direction origin, Direction destination);
/*
bool 
go_straight(Direction origin, Direction destination) 
{
  if (((origin == north) && (destination == south)) ||
      ((origin == south) && (destination == north)) ||
      ((origin == east) && (destination == west)) ||
      ((origin == west) && (destination == east))) {
    return true;
  } else {
    return false;
  }
}
*/
/*
bool
turn_left(Direction origin, Direction destination) {
  //KASSERT(v != NULL);
  if (((origin == north) && (destination == east)) ||
      ((origin == east) && (destination == south)) ||
      ((origin == south) && (destination == west)) ||
      ((origin == west) && (destination == north))) {
    return true;
  } else {
    return false;
  }
}
*/
bool
turn_right(Direction origin, Direction destination) {
  
  if (((origin == west) && (destination == south)) ||
      ((origin == south) && (destination == east)) ||
      ((origin == east) && (destination == north)) ||
      ((origin == north) && (destination == west))) {
    return true;
  } else {
    return false;
  }
}
// Collision case when there's at least one car turn left
bool
check_invalid_condition(Direction origin, Direction destination)
{
  for(unsigned int i = 0; i < array_num(vehicles); ++i) {
    Vehicle *now = array_get(vehicles, i);
    Direction tempO = now->origin;
    Direction tempD = now->destination;
/*
    if ((tempD == destination) && (turn_right(origin, destination) || turn_right(tempO, tempD)))     {
	return true;
    }
    if ((turn_left(tempO, tempD) || turn_left(origin, destination)) && 
        (origin != tempO))
    {
        return true;
    }
    if ((go_straight(tempO, tempD) && go_straight(origin, destination)) && 
        (origin != tempO) && (origin != tempD))
    {
        return true;
    }
*/
    if ((origin == tempO) || ((origin == tempD) && (destination == tempO)) ||
       ((destination != tempD) && (turn_right(origin, destination) || turn_right(tempO, tempD))))
    {
	continue;
    }
    return true;
  }
  return false;
}
/*
bool
check_invalid_conditionB(Direction origin, Direction destination)
{
  for(unsigned int i = 0; i < array_num(vehicles); ++i) {
    Vehicle *now = array_get(vehicles, i);
    Direction tempO = now->origin;
    Direction tempD = now->destination;
    if ((go_straight(tempO, tempD) && go_straight(origin, destination)) && 
        (origin != tempO) && (origin != tempD))
    {
        return true;
    }
  }
  return false;
}
*/
/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
  /* replace this default implementation with your own implementation */
  traffic_lock = lock_create("traffic lock");
  if (traffic_lock == NULL) {
    panic("could not create intersection lock");
  }
  go = cv_create("go");
  if (go == NULL) {
    panic("could not create conditional variable");
  }
/*  noLeft = cv_create("noLeft");
  if (noLeft == NULL) {
    panic("could not create conditional variable");
  }
  TurnRight = cv_create("TurnRight");
  if (TurnRight == NULL) {
    panic("could not create conditional variable");
  }
  NoWay = cv_create("NoWay");
  if (NoWay == NULL) {
    panic("could not create conditional variable");
  }
*/
  vehicles = array_create();
  return;
}
/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
  KASSERT(traffic_lock != NULL);
  KASSERT(go != NULL);
  //KASSERT(noLeft != NULL);
  //KASSERT(TurnRight != NULL);
  //KASSERT(NoWay != NULL);
  lock_destroy(traffic_lock);
  array_destroy(vehicles);
  cv_destroy(go);
}
/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */
void
intersection_before_entry(Direction origin, Direction destination) 
{
  KASSERT(traffic_lock != NULL);
  lock_acquire(traffic_lock);
  while (check_invalid_condition(origin, destination))
  {
    cv_wait(go,traffic_lock);  
  }
  /*while (check_invalid_conditionB(origin, destination))  
  {
    cv_wait(noLeft,traffic_lock);
  }*/
  Vehicle *v = kmalloc(sizeof(Vehicle));
  v->origin = origin;
  v->destination = destination;
  array_add(vehicles, v, NULL);
  lock_release(traffic_lock);
}
/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */
void
intersection_after_exit(Direction origin, Direction destination) 
{
  KASSERT(traffic_lock != NULL);
  KASSERT(go != NULL);
  //KASSERT(noLeft != NULL);
  lock_acquire(traffic_lock);
  for (unsigned int i = 0; i < array_num(vehicles); ++i)
  {
    Vehicle *now = array_get(vehicles, i);
    if (now->origin == origin && now->destination == destination)
    {
   
	//cv_broadcast(turnLeft, traffic_lock);
	cv_broadcast(go, traffic_lock);
	array_remove(vehicles, i);
	break;
    }
  }
  lock_release(traffic_lock);
}
