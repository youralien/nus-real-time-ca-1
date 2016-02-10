#include "apples.h"
#include <stdio.h>
#include <time.h>
#include <pthread.h>

#include <sys/ipc.h>
#include <sys/msg.h>


int num_apples = 20;    // Number of Apples to Process, to shorten the test
int TIME2ACT = 5;       // Time to Act between taking photo and discarding in seconds

struct photo_msgbuf {
    long mtype;
    struct photo_and_time {
        PHOTO photo;
        time_t time_start;
    } mdata;
};

struct quality_msgbuf {
    long mtype;
    struct quality_and_time {
        QUALITY quality;
        time_t time_start; 
    } mdata; 
};

/* timing helpers */
time_t get_time_now() {
    time_t now;
    now = time(&now);
    return now;
}

double get_time_elapsed(time_t t0) {
    time_t now = get_time_now();
    double time_elapsed = difftime(now, t0);
    return time_elapsed;
}

/* message queues */
int PHOTO_TYPE = 1;
int QUALITY_TYPE = 1;
int PHOTO_QID;
int QUALITY_QID;
void mq_init(void) {
    // key is a unique identifier
    PHOTO_QID = msgget(PHOTO_TYPE,IPC_CREAT|0666);
    QUALITY_QID = msgget(QUALITY_TYPE,IPC_CREAT|0666);
}


/* thread managers and thread processes */
pthread_t manager1;
pthread_t manager2;
pthread_t manager3;

void *manage_photo_taking(void *p) {
    while (more_apples() && num_apples >= 0) {
        num_apples--;
        
        // wait til you can take the photo
        wait_until_apple_under_camera();
        
        // timeStart
        time_t time_start = get_time_now();
        
        // take the photo
        PHOTO photo = take_photo();
    
        // push to photo message queue
        struct photo_msgbuf mbuf = {PHOTO_TYPE, {photo, time_start}};
        msgsnd(PHOTO_QID, &mbuf, sizeof(struct photo_msgbuf), IPC_NOWAIT);

        printf("Taker!\n");
        // Wait until apple has passed so we can get the next apple!
        usleep(500 * 1000); // 500ms 
    }
}

void *manage_photo_processing(void *p) {
    while (more_apples() && num_apples >= 0) {

        // pop from the photo message queue
        struct photo_msgbuf mbuf; 
        msgrcv(PHOTO_QID, &mbuf, sizeof(struct photo_msgbuf), PHOTO_TYPE, MSG_NOERROR);
        double time_elapsed = get_time_elapsed(mbuf.mdata.time_start);

        printf("   Processor!    %f \n", time_elapsed);
        // we still have time to act and process
        if (time_elapsed < TIME2ACT) {
            // send the photo to the Image Processing Unit
            QUALITY quality = process_photo(mbuf.mdata.photo);

            // push to quality message queue
            struct quality_msgbuf mbuf_quality = {
                QUALITY_TYPE,
                {quality, mbuf.mdata.time_start}
            };
            msgsnd(QUALITY_QID, &mbuf_quality, sizeof(struct quality_msgbuf), IPC_NOWAIT);
        }
        else {
            printf("... blocked ... \n");
        }
    }    
}

void *manage_actuator(void *p) {
    while (more_apples() && num_apples >= 0) {
        
        // pop from the quality message queue
        struct quality_msgbuf mbuf;
        msgrcv(QUALITY_QID, &mbuf, sizeof(struct quality_msgbuf), PHOTO_TYPE, MSG_NOERROR);

        double time_elapsed = get_time_elapsed(mbuf.mdata.time_start);
        printf("      Actuator! %f \n", time_elapsed);

        if (mbuf.mdata.quality == BAD && time_elapsed <= 5) {
            printf("Discard BAD");
            discard_apple();
        }
        else if (mbuf.mdata.quality == UNKNOWN) {
            printf("Discard UNKNOWN");
            discard_apple();
        }
        else {
           // do nothing
           return;
        }
    }
}

int main () {
    start_test();
    
    mq_init();
    
    pthread_create(&manager1, NULL, manage_photo_taking, NULL);    
    pthread_create(&manager2, NULL, manage_photo_processing, NULL);    
    pthread_create(&manager3, NULL, manage_actuator, NULL);    
    // wait for all of them to complete
    pthread_join(manager1, NULL);
    pthread_join(manager2, NULL);
    pthread_join(manager3, NULL);
    
    end_test();
    return 0;
}
