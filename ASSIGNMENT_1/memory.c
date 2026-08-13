#include<stdio.h>
#include "memory.h"

unsigned char Instruction[256];
unsigned char Data[256];

void initialize() {
    FILE *fileptr;
    fileptr=fopen("program.byte","r");
    int i=0;

    // while(fscanf(fileptr,"%hhu",&Instruction[i])==1){
	while(fscanf(fileptr, "%hhu",&Instruction[i])==1){
       		i++;
	}

    fclose(fileptr);
    fileptr=fopen("data.byte","r");
    i=0;

//    while(fileptr && fscanf(fileptr,"%hhu",&Data[i])==1){
	while(fileptr&& fscanf(fileptr, "%hhu", &Data[i])==1){
        	i++;
	}
    if(fileptr){
		fclose(fileptr);
	}
}

void finalize() {
    FILE *fileptr=fopen("data.byte","w");

    for(int i=0;i<256;i++){
        	fprintf(fileptr,"%d ",Data[i]);
	}

	fclose(fileptr);
}
