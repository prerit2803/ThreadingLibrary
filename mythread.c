#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include "mythread.h"
#define SIZE 1024*8
typedef struct MyThread _MyThread;
int toggle=0;
typedef struct Qnode
{
	_MyThread *th;
	struct Qnode *next;
}Qnode;

typedef struct Q{
	Qnode *front;
	Qnode *rear;
}Q;

struct MyThread
{
	ucontext_t ctxt;
	struct MyThread *parent;
	struct MyThread *JoinChild;
	Q *child;
};
typedef struct Semaphore{
	int count;
	Q *queue;
}Sem;

void QueueInit(Q *q){
	q->front=NULL;
	q->rear=NULL;
}
_MyThread *currentThread;
ucontext_t InitialContext;
Q *readyQ, *blockedQ;


void Enqueue(Q *q, _MyThread *thread){
	Qnode *newNode, *temp;
	newNode = (Qnode *)malloc(sizeof(Qnode));

	if (newNode==NULL)
		return;

	newNode->th=thread;
	newNode->next=NULL;

	if (q->front==NULL && q->rear==NULL)
	{
		q->front=newNode;
		q->rear=newNode;
	}
	else
	{
		q->rear->next=newNode;
		q->rear=newNode;
	}
}
_MyThread* Dequeue(Q *q){
	Qnode *temp;
	_MyThread *thread;

	temp=q->front;
	if (temp==NULL)
	{
		return NULL;
	}
	if (q->front==q->rear)
	{
		q->front=NULL;
		q->rear=NULL;
	}
	else{
		if (q->front==NULL)
			return NULL;
		else
			q->front=q->front->next;
	}
	thread = temp->th;
	free(temp);

	return thread;
}
int IsEmpty(Q *q){
	if (q->front==NULL)
		return 1; //empty return true
	else
		return 0; //not empty
}
int IsFound(Q *q, _MyThread *th){
	Qnode *temp;
	if (q->front==NULL)
	{
		printf("Queue is empty\n");
		return 0;
	}
	temp=q->front;
	if(temp->th==th)
		return 1;
	while(temp->next!=NULL){
		if(temp->th==th)
		{
			return 1;
		}
		temp=temp->next;
	}
	return 0;
}

void MyThreadInit (void(*start_funct)(void *), void *args){
	//InitialContext=(ucontext_t *)malloc(sizeof(ucontext_t));
	if(toggle==1)
	{
		printf("MyThreadInit can be called only once\n");
		return;
	}
	toggle=1;
	getcontext(&InitialContext);

	_MyThread *InitThread=(_MyThread *)malloc(sizeof(_MyThread));
	
	//InitThread->ctxt=(ucontext_t *)malloc(sizeof(ucontext_t));
	getcontext(&InitThread->ctxt);
	InitThread->ctxt.uc_link=0;
 	InitThread->ctxt.uc_stack.ss_sp=malloc(SIZE);
 	InitThread->ctxt.uc_stack.ss_size=SIZE;
 	InitThread->ctxt.uc_stack.ss_flags=0;

	InitThread->parent=NULL;
	InitThread->JoinChild=NULL;
	InitThread->child=(Q *)malloc(sizeof(Q));
	QueueInit(InitThread->child);

	makecontext(&InitThread->ctxt,(void(*) ())start_funct,1,args);
	currentThread=(_MyThread *)malloc(sizeof(_MyThread));
	currentThread=InitThread;

	readyQ=(Q *)malloc(sizeof(Q));
	blockedQ=(Q *)malloc(sizeof(Q));
	QueueInit(readyQ);
	QueueInit(blockedQ);

	swapcontext(&InitialContext, &InitThread->ctxt);
}

MyThread MyThreadCreate (void(*start_funct)(void *), void *args)
{
	_MyThread *MyTh=(_MyThread *)malloc(sizeof(_MyThread));
	getcontext(&MyTh->ctxt);
	MyTh->ctxt.uc_link=0;
 	MyTh->ctxt.uc_stack.ss_sp=malloc(SIZE);
 	MyTh->ctxt.uc_stack.ss_size=SIZE;
 	MyTh->ctxt.uc_stack.ss_flags=0;

	MyTh->parent=currentThread;
	MyTh->JoinChild=NULL;
	MyTh->child=(Q *)malloc(sizeof(Q));
	QueueInit(MyTh->child);
	Enqueue(currentThread->child, MyTh);
	Enqueue(readyQ, MyTh);

	makecontext(&MyTh->ctxt,(void(*) ())start_funct,1,args);
	return (MyThread)MyTh;
}

void MyThreadYield(void)
{
	_MyThread *temp;
	temp=currentThread;
	if (IsEmpty(readyQ)==1)
	{
		return;
	}
	Enqueue(readyQ,currentThread);
	currentThread=Dequeue(readyQ);
	if (currentThread==NULL)
	{
		return;
	}
	getcontext(&temp->ctxt);
	swapcontext(&temp->ctxt, &currentThread->ctxt);
}
int MyThreadJoin(MyThread thread){
	_MyThread *MyChild = (_MyThread *)thread;
	if (MyChild->parent!=currentThread)
	{
		return -1;
	}
	if (IsFound(currentThread->child, MyChild)==1)
	{
		currentThread->JoinChild=MyChild;
		Enqueue(blockedQ, currentThread);
		_MyThread *temp;
		temp=currentThread;
		currentThread=Dequeue(readyQ);
		getcontext(&temp->ctxt);
		swapcontext(&temp->ctxt, &currentThread->ctxt);
	}
	else{
		printf("child already terminated\n");
	}
	return 0;
}

void MyThreadJoinAll(void){
	if(IsEmpty(currentThread->child)!=1){
		Enqueue(blockedQ, currentThread);
		_MyThread *temp;
		temp=currentThread;
		temp=Dequeue(readyQ);
		currentThread->JoinChild=temp;
		currentThread=temp;
		getcontext(&temp->ctxt);
		swapcontext(&temp->ctxt, &currentThread->ctxt);
	}
	else{
		return;
	}
}
void MyThreadExit(void){
	if (currentThread==NULL)
		return;

	Qnode *temp = currentThread->child->front;

	if(currentThread->parent!=NULL && IsEmpty(blockedQ)==0 && IsFound(blockedQ, currentThread->parent)==1){
		_MyThread *t = Dequeue(currentThread->parent->child);
		if (currentThread->parent->JoinChild==t || IsEmpty(currentThread->parent->child)==1){
			_MyThread *parent=Dequeue(blockedQ);
			Enqueue(readyQ,parent);
			parent->JoinChild=NULL;
		}
	}
	
	if(IsEmpty(readyQ)==1)
	{
		setcontext(&InitialContext);
		return;
	}
	free(currentThread);
	currentThread=Dequeue(readyQ);
	if (currentThread==NULL)
	{
		return;
	}
	while(temp!=NULL){
		temp->th->parent=NULL;
		if(temp->next==NULL){
			break;
		}
		temp=temp->next;
	}
	setcontext(&currentThread->ctxt);
}

MySemaphore MySemaphoreInit(int initialValue){
	Sem *MySem;
	MySem=(MySemaphore)MySem;
	MySem = (Sem *)malloc(sizeof(Sem));
	
	if (MySem==NULL)//error handling for semaphore
		return NULL;
	if (initialValue < 0)//error handling for negative value of initial Value
		return NULL;

	MySem->queue = (Q *)malloc(sizeof(Q));
	if (MySem->queue==NULL)//error handling for semaphore queue
	{
		printf("Semaphore Queue is empty\n");
		return NULL;
	}
	MySem->count=initialValue;
	QueueInit(MySem->queue);

	return MySem;
}

void MySemaphoreSignal(MySemaphore sem){
	Sem *MySem=(Sem *)sem;
	_MyThread *MyTh;
	(MySem->count)++;

	if(MySem->count <= 0){
		if (IsEmpty(MySem->queue)==1)//check if semaphore queue is empty.
		{
			printf("Semaphore Queue is empty\n");
			return;
		}
		MyTh=Dequeue(MySem->queue);
		Enqueue(readyQ, MyTh);
	}
}

void MySemaphoreWait(MySemaphore sem){
	Sem *MySem=(Sem *)sem;
	_MyThread *MyTh, *running;
	(MySem->count)--;
	running=currentThread;
	if(MySem->count < 0){
		Enqueue(MySem->queue ,currentThread);
		MyTh=Dequeue(readyQ);
		currentThread=MyTh;
		swapcontext(&running->ctxt, &MyTh->ctxt);
	}
}

int MySemaphoreDestroy(MySemaphore sem){
	Sem *MySem=(Sem *)sem;
	if (IsEmpty(MySem->queue)==0)
	{
		return -1;
	}
	else{
		MySem->queue=NULL;
		return 0;
	}
}