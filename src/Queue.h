/*
 * Queue.h
 *
 * Created: 8/10/2018 10:41:26
 *  Author: MQUEZADA
 */ 


#ifndef QUEUE_H_
#define QUEUE_H_
#include <stdlib.h>
#include <stdint.h>

// define default capacity of the queue
#define SIZE 128

class Node
{
   public:
      Node *inFront = NULL;
      Node *behind = NULL;
      uint8_t data = 0;
};

// Class for queue
class queue
{
   uint16_t capacity;	// maximum capacity of the queue
   Node *front;		// front points to front element in the queue (if any)
   Node *rear;		// rear points to last element in the queue
   uint16_t count;		// current size of the queue

   public:
   queue(uint16_t size = SIZE);		// constructor

   uint8_t pop();
   void push(uint8_t x);
   uint8_t peek();
   uint16_t size();
   bool isEmpty();
   bool isFull();
};


#endif /* QUEUE_H_ */