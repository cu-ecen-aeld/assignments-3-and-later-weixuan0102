#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>

int main(int argc, char *argv[]){
	openlog(NULL,0,LOG_USER);
	if(argc != 3){
		syslog(LOG_ERR, "Invalid Number of arguments: %d",argc - 1);
		return 1;
	}
	
	FILE * fp = NULL;
	fp = fopen(argv[1], "w");
	if(fp != NULL){
		fprintf(fp, "%s",argv[2]);
       	 	fclose(fp);
        	syslog(LOG_DEBUG,"Writing %s to %s", argv[2], argv[1]);
        	return 0;
	}else{
		syslog(LOG_ERR,"Error: %s", strerror(errno));
		return 1;
	}
	fclose(fp);
	return 0;
}
