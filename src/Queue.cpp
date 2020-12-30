/*
 * Queue.cpp
 *
 * Created: 8/10/2018 10:41:03
 *  Author: MQUEZADA
 */ 
#include "Queue.h"

// Constructor to initialize queue
queue::queue(uint16_t size)
{
   capacity = size;
   front = NULL;
   rear = NULL;
   count = 0;
}

queue::~queue()
{
   clear();
}

void queue::clear()
{
   while(isEmpty() == false)
   {
      pop();
   }
}

// Utility function to remove front element from the queue
uint8_t queue::pop()
{
   uint8_t data = 0;
   Node *ret = NULL;
   // check for queue underflow
   if (isEmpty() == false)
   {
      //front = (front + 1) % capacity;
      ret = front;
      front = ret->behind;
      if(front == NULL)
      {
         rear = NULL;
      }
      data = ret->data;
      free(ret);
      count--;
   }
   return(data);
}

// Utility function to add an item to the queue
void queue::push(uint8_t data)
{
   Node *item = (Node *)calloc(1, sizeof(Node));
   item->data = data;
   // check for queue overflow
   if (isFull() == false)
   {
      //rear = (rear + 1) % capacity;
      //arr[rear] = item;
      if(front == NULL)
      {
         front = item;
      }
      
      if(rear != NULL)
      {
         rear->behind = item;
         item->inFront = rear;
      }
      rear = item;
      count++;
   }      
}

// Utility function to return front element in the queue
uint8_t queue::peek()
{
   if (isEmpty() == false)
   {
      return front->data;
   }
   return(0);      
}

// Utility function to return the size of the queue
uint16_t queue::size()
{
   return count;
}

// Utility function to check if the queue is empty or not
bool queue::isEmpty()
{
   return (size() == 0);
}

// Utility function to check if the queue is full or not
bool queue::isFull()
{
   return (size() == capacity);
}

uint16_t queue::sum()
{
   uint16_t s(0);
   Node *t(front);
   while(t != rear)
   {
      if(t == NULL)
      {
         break;
      }
      s += t->data;
      t = t->behind;
   }
   return(s);
}
