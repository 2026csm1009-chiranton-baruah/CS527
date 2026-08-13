#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include "compiler.h"

void compile() {
    FILE *in = fopen("program.txt","r");
    FILE *out = fopen("program.byte","w");
    char line[100];

    while(fgets(line,sizeof(line),in)) {
        int d,s1,s2,c;

        if(sscanf(line,"Read x%d , %d",&d,&c)==2 || sscanf(line,"Read x%d, %d",&d,&c)==2) {
            fprintf(out,"5 %d %d 0\n",d,c);
        }

        else if(sscanf(line,"Write x%d , %d",&d,&c)==2 || sscanf(line,"Write x%d, %d",&d,&c)==2) {
            fprintf(out,"6 %d %d 0\n",d,c);
        }

        else if(sscanf(line,"x%d = %d",&d,&c)==2) {
            fprintf(out,"7 %d %d 0\n",d,c);
        }

        else {
            char op;

            if(sscanf(line,"x%d = x%d %c x%d",&d,&s1,&op,&s2)==4) {
                int opcode=0;

                switch(op) {
                    case '+': opcode=1; break;
                    case '-': opcode=2; break;
                    case '*': opcode=3; break;
                    case '/': opcode=4; break;
                }

                fprintf(out,"%d %d %d %d\n",opcode,d,s1,s2);
            }
        }
    }

    fprintf(out,"0 0 0 0\n");

    fclose(in);
    fclose(out);
}
