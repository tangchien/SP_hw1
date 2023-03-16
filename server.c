//	Reference:b09902008
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    // you don't need to change this.
    int id;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list
struct pollfd fds[25];
int write_fds[25];
int lock[25] = {0};
int fd_num;
int reg_fd;

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

typedef struct {
    int id;          //902001-902020
    int AZ;          
    int BNT;         
    int Moderna;     
}registerRecord;


int handle_read(request* reqP) {
    int r;
    char buf[512];

    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012");
    int newline_len = 2;
    if (p1 == NULL) {
       p1 = strstr(buf, "\012");
        if (p1 == NULL) {
            ERR_EXIT("this really should not happen...");
        }
    }
    size_t len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len-1;
    return 1;
}

int is_valid(request* reqP){
    if (reqP->buf_len != 6){
        return 0;
    }
    int val = atoi(reqP->buf);
    if (val >= 902001 && val <= 902020){
        reqP->id = val;
        return val;
    }
    return 0;
}

int read_part(request* reqP){
    //  check is valid
    if (is_valid(reqP)){
        int cli_id = is_valid(reqP)-902001;
        int r_data[10];
        lseek(reg_fd,sizeof(registerRecord)*cli_id,0);
        int ret_read = read(reg_fd,&r_data,sizeof(registerRecord));
        int az = r_data[1], bnt = r_data[2], moderna = r_data[3];
        printf("az = %d, bnt = %d, moderna = %d\n",az,bnt,moderna);
	    char buf_start[50] = "Your preference order is";
        write(reqP->conn_fd, buf_start, strlen(buf_start));
        switch(az){
            case (1):
                if (bnt > moderna){
                    char buf_end[50] = " AZ > Moderna > BNT.\n";
                    write(reqP->conn_fd,buf_end,strlen(buf_end));
                }
                else{
                    char buf_end[50] = " AZ > BNT > Moderna.\n";
                    write(reqP->conn_fd,buf_end,strlen(buf_end));
                }
                break;
            case (2):
                if (bnt > moderna){
                    char buf_end[50] = " Moderna > AZ > BNT.\n";
                    write(reqP->conn_fd,buf_end,strlen(buf_end));
                }
                else{
                    char buf_end[50] = " BNT > AZ > Moderna.\n";
                    write(reqP->conn_fd,buf_end,strlen(buf_end));
                }
                break;
            case (3):
                if (bnt > moderna){
                    char buf_end[50] = " Moderna > BNT > AZ.\n";
                    write(reqP->conn_fd,buf_end,strlen(buf_end));
                }
                else{
                    char buf_end[50] = " BNT > Moderna > AZ.\n";
                    write(reqP->conn_fd,buf_end,strlen(buf_end));
                }
                break;
            default :
                printf("this should not happen\n");
        }
        return 1;
    }
    else{
        char buf_error[100] = "[Error] Operation failed. Please try again.\n";
        write(reqP->conn_fd,buf_error,strlen(buf_error));
        return -1;
    }

}

void print_error(request* reqP){
    char buf_error[100] = "[Error] Operation failed. Please try again.\n";
    write(reqP->conn_fd,buf_error,strlen(buf_error));
    return;
}

void print_lk(request* reqP){
    char buf_error[10] = "Locked.\n";
    write(reqP->conn_fd,buf_error,strlen(buf_error));
    return;
}

int write_part(request* reqP){
    //  az bnt moderna
    int az, bnt, moderna,space;
    if (reqP->buf_len != 5){
        print_error(reqP);
        return -1;
    }
    for (int i = 0;i < 5;i++){
        switch(i){
            case (0):
                az = (int)reqP->buf[0];
                if (az < 49 || az > 51){
                    print_error(reqP);
                    return -1;
                }
                break;
            case (1):
                space = (int)reqP->buf[1];
                if (space != 32){
                    print_error(reqP);
                    return -1;
                }
                break;
            case (2):
                bnt = (int)reqP->buf[2];
                if (bnt < 49 || bnt > 51 || bnt == az){
                    print_error(reqP);
                    return -1;
                }
                break;
            case (3):
                space = (int)reqP->buf[3];
                if (space != 32){
                    print_error(reqP);
                    return -1;
                }
                break;
            case (4):
                moderna = (int)reqP->buf[4];
                if (moderna < 49 || moderna > 51 || moderna == az || moderna == bnt){
                    print_error(reqP);
                    return -1;
                }
                break;
            default:
                printf("this should not happened\n");
        }
    }
    char buf1[30] = "Preference order for ";
    write(reqP->conn_fd,buf1,strlen(buf1));
    int uid = reqP->id;
    char cid[10] = "9020";
    uid %= 100;
    cid[4] = (char)(uid/10+48);
    cid[5] = (char)(uid%10+48);
    write(reqP->conn_fd,cid,strlen(cid));
    char buf2[50] = " modified successed, new preference order is";
    write(reqP->conn_fd,buf2,strlen(buf2));
    switch(az){
        case (49):
            if (bnt > moderna){
                char buf_end[50] = " AZ > Moderna > BNT.\n";
                write(reqP->conn_fd,buf_end,strlen(buf_end));
            }
            else{
                char buf_end[50] = " AZ > BNT > Moderna.\n";
                write(reqP->conn_fd,buf_end,strlen(buf_end));
            }
            break;
        case (50):
            if (bnt > moderna){
                char buf_end[50] = " Moderna > AZ > BNT.\n";
                write(reqP->conn_fd,buf_end,strlen(buf_end));
            }
            else{
                char buf_end[50] = " BNT > AZ > Moderna.\n";
                write(reqP->conn_fd,buf_end,strlen(buf_end));
            }
            break;
        case (51):
            if (bnt > moderna){
                char buf_end[50] = " Moderna > BNT > AZ.\n";
                write(reqP->conn_fd,buf_end,strlen(buf_end));
            }
            else{
                char buf_end[50] = " BNT > Moderna > AZ.\n";
                write(reqP->conn_fd,buf_end,strlen(buf_end));
            }
            break;
        default :
            printf("this should not happen\n");
    }
    int n_block = reqP->id-902001;
    lseek(reg_fd,sizeof(registerRecord)*n_block,SEEK_SET);
    registerRecord update_data = {reqP->id,az-48,bnt-48,moderna-48};
    int test = write(reg_fd,&update_data,sizeof(registerRecord));
    printf("test = %d\n",test);
    return 1;
}

int main(int argc, char** argv) {
    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    for (int i = 0;i < 25;i++){
        write_fds[i] = 0;
    }
    
    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;
    int timeout = -1;
    reg_fd = open("registerRecord",O_RDWR);
    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);
    fds[0].fd = svr.listen_fd;
    fds[0].events = POLLIN;
    fd_num = 1;
    int ready_q[25];
    int ready_temp = 0,ready_num;
    while (1) {
        // TODO: Add IO multiplexing
	int total_fd_num = fd_num;
	ready_temp = 0;
        ready_num = poll(fds,fd_num,-1);
        for (int i = 0;i < total_fd_num;i++){
            if (fds[i].revents == POLLIN && i == 0){     //  new_connection
                conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
                if (conn_fd < 0) {
                    if (errno == EINTR || errno == EAGAIN) continue;  // try again
                    if (errno == ENFILE) {
                        (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                        continue;
                    }
                    ERR_EXIT("accept");
                }
                requestP[conn_fd].conn_fd = conn_fd;
                strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
                char buf1[100] = "Please enter your id (to check your preference order):\n";
                write(conn_fd,buf1,strlen(buf1));
                fds[fd_num].fd = conn_fd;
                fds[fd_num].events = POLLIN;
                fd_num++;
                ready_num--;
            }
            else if (fds[i].revents == POLLIN){          //  ready
		        int ret = handle_read(&requestP[fds[i].fd]);
                fprintf(stderr, "ret = %d\n", ret);
                if (ret < 0) {
                    fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
                    continue;
                }
		        printf("input_data = %s\n",requestP[fds[i].fd].buf);
                ready_q[ready_temp++] = i;
            }
        }
        // Check new connection
        /*
        clilen = sizeof(cliaddr);
        
        

        int ret = handle_read(&requestP[conn_fd]); // parse data from client to requestP[conn_fd].buf
        fprintf(stderr, "ret = %d\n", ret);
	    if (ret < 0) {
            fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
            continue;
        }
        */

    // TODO: handle requests from clients
#ifdef READ_SERVER 
        int success_read;     
        for (int i = 0;i < ready_temp;i++){
            if (is_valid(&requestP[fds[ready_q[i]].fd]) != 0){
                //  lock
                printf("lock[%d] = %d\n",is_valid(&requestP[fds[ready_q[i]].fd])-902001,lock[is_valid(&requestP[fds[ready_q[i]].fd])-902001]);
                if (lock[is_valid(&requestP[fds[ready_q[i]].fd])-902001] > 1){
                    print_lk(&requestP[fds[ready_q[i]].fd]);
                    close(requestP[fds[ready_q[i]].fd].conn_fd);
                    free_request(&requestP[fds[ready_q[i]].fd]);
                    if (ready_q[i] == fd_num-1){
                        fd_num--;
                    }
                    else{
                        fds[ready_q[i]].fd = fds[fd_num-1].fd;
                        fds[ready_q[i]].events = POLLIN;
                        fd_num--;
                    }
                    continue;
                }
                int l = is_valid(&requestP[fds[ready_q[i]].fd]) - 902001;
                struct flock set_rd_lk;
                set_rd_lk.l_type = F_RDLCK;
                set_rd_lk.l_whence = SEEK_SET;
                set_rd_lk.l_start = l*sizeof(registerRecord);
                set_rd_lk.l_len = sizeof(registerRecord);
                int sudstlk = fcntl(reg_fd, F_SETLK, &set_rd_lk);
                if (sudstlk == -1){
                    print_lk(&requestP[fds[ready_q[i]].fd]);
                    close(requestP[fds[ready_q[i]].fd].conn_fd);
                    free_request(&requestP[fds[ready_q[i]].fd]);
                    if (ready_q[i] == fd_num-1){
                        fd_num--;
                    }
                    else{
                        fds[ready_q[i]].fd = fds[fd_num-1].fd;
                        fds[ready_q[i]].events = POLLIN;
                        fd_num--;
                    }
                    continue;
                }
                lock[is_valid(&requestP[fds[ready_q[i]].fd])-902001] = 1;
                success_read = read_part(&requestP[fds[ready_q[i]].fd]);
                //  unlocked
                struct flock free_rd_lk;
                free_rd_lk.l_type = F_UNLCK;
                free_rd_lk.l_whence = SEEK_SET;
                free_rd_lk.l_start = l*sizeof(registerRecord);
                free_rd_lk.l_len = sizeof(registerRecord);
                int sudfreelk = fcntl(reg_fd, F_SETLK, &free_rd_lk);
                if (sudfreelk == -1){
                    printf("fail to unlock RDLCK\n");
                }
                lock[ready_q[i]] = 0;
            }
            else{
                print_error(&requestP[fds[ready_q[i]].fd]);
            }
            close(requestP[fds[ready_q[i]].fd].conn_fd);
            free_request(&requestP[fds[ready_q[i]].fd]);
            if (ready_q[i] == fd_num-1){
                fd_num--;
            }
            else{
                fds[ready_q[i]].fd = fds[fd_num-1].fd;
                fds[ready_q[i]].events = POLLIN;
                fd_num--;
            }
        }   
        //  finish connect
#elif defined WRITE_SERVER
        int succeed_write1,succeed_write2;
        for (int i = 0;i < ready_temp;i++){
            if (requestP[fds[ready_q[i]].fd].id >= 902001 || requestP[fds[ready_q[i]].fd].id <= 902020 || is_valid(&requestP[fds[ready_q[i]].fd]) != 0){
                if (write_fds[ready_q[i]] == 0){
                    if (is_valid(&requestP[fds[ready_q[i]].fd]) != 0){
                        if (lock[is_valid(&requestP[fds[ready_q[i]].fd])-902001] != 0){
                            print_lk(&requestP[fds[ready_q[i]].fd]);
                            close(requestP[fds[ready_q[i]].fd].conn_fd);
                            free_request(&requestP[fds[ready_q[i]].fd]);
                            if (ready_q[i] == fd_num-1){
                                write_fds[fd_num-1] = 0;
                                fd_num--;
                            }
                            else{
                                fds[ready_q[i]].fd = fds[fd_num-1].fd;
                                fds[ready_q[i]].events = POLLIN;
                                write_fds[ready_q[i]] = write_fds[fd_num-1];
                                write_fds[fd_num-1] = 0;
                                fd_num--;
                            }
                            continue;
                        }
                        lock[is_valid(&requestP[fds[ready_q[i]].fd])-902001] = 2;
                        int l = is_valid(&requestP[fds[ready_q[i]].fd])-902001;
                        struct flock stwrlk;
                        stwrlk.l_type = F_WRLCK;
                        stwrlk.l_whence = SEEK_SET;
                        stwrlk.l_start = l*sizeof(registerRecord);
                        stwrlk.l_len = sizeof(registerRecord);
                        int scdwr = fcntl(reg_fd,F_SETLK,&stwrlk);
                        printf("wr return value = %d\n",scdwr);
                        if (scdwr == -1){
                            print_lk(&requestP[fds[ready_q[i]].fd]);
                            close(requestP[fds[ready_q[i]].fd].conn_fd);
                            free_request(&requestP[fds[ready_q[i]].fd]);
                            if (ready_q[i] == fd_num-1){
                                write_fds[fd_num-1] = 0;
                                fd_num--;
                            }
                            else{
                                fds[ready_q[i]].fd = fds[fd_num-1].fd;
                                fds[ready_q[i]].events = POLLIN;
                                write_fds[ready_q[i]] = write_fds[fd_num-1];
                                write_fds[fd_num-1] = 0;
                                fd_num--;
                            }
                            continue;
                        }
                        succeed_write1 = read_part(&requestP[fds[ready_q[i]].fd]);
                        if (succeed_write1 == 1){       //  valid input
                            char buf_change[100] = "Please input your preference order respectively(AZ,BNT,Moderna):\n";
                            write(requestP[fds[ready_q[i]].fd].conn_fd,buf_change,strlen(buf_change));
                            write_fds[ready_q[i]]++;
                        }
                    }
                    else{           //  close the fd
                        print_error(&requestP[fds[ready_q[i]].fd]);
                        close(requestP[fds[ready_q[i]].fd].conn_fd);
                        free_request(&requestP[fds[ready_q[i]].fd]);
                        if (ready_q[i] == fd_num-1){
                            write_fds[fd_num-1] = 0;
                            fd_num--;
                        }
                        else{
                            fds[ready_q[i]].fd = fds[fd_num-1].fd;
                            fds[ready_q[i]].events = POLLIN;
                            write_fds[ready_q[i]] = write_fds[fd_num-1];
                            write_fds[fd_num-1] = 0;
                            fd_num--;
                        }
                    }
                }
                else{
                    //  close fd
                    succeed_write2 = write_part(&requestP[fds[ready_q[i]].fd]);
                    //  unlocked
                    int l = requestP[fds[ready_q[i]].fd].id - 902001;
                    struct flock unwrlk;
                    unwrlk.l_type = F_UNLCK;
                    unwrlk.l_whence = SEEK_SET;
                    unwrlk.l_start = l*sizeof(registerRecord);
                    unwrlk.l_len = sizeof(registerRecord);
                    int scdunwr = fcntl(reg_fd, F_SETLK, &unwrlk);
                    if (scdunwr == -1){
                        printf("fail to unlock write lock\n");
                    }
                    lock[requestP[fds[ready_q[i]].fd].id-902001] = 0;
                    close(requestP[fds[ready_q[i]].fd].conn_fd);
                    free_request(&requestP[fds[ready_q[i]].fd]);
                    if (ready_q[i] == fd_num-1){
                        write_fds[fd_num-1] = 0;
                        fd_num--;
                    }
                    else{
                        fds[ready_q[i]].fd = fds[fd_num-1].fd;
                        fds[ready_q[i]].events = POLLIN;
                        write_fds[ready_q[i]] = write_fds[fd_num-1];
                        write_fds[fd_num-1] = 0;
                        fd_num--;
                    }
                }
            }
            else{
                print_error(&requestP[fds[ready_q[i]].fd]);
            }            
        }
        //  fprintf(stderr, "%s", requestP[conn_fd].buf);
        //  sprintf(buf,"%s : %s",accept_write_header,requestP[conn_fd].buf);
        //  write(requestP[conn_fd].conn_fd, buf, strlen(buf));    
#endif

        //	close(requestP[conn_fd].conn_fd);
        //	free_request(&requestP[conn_fd]);
    }
    free(requestP);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}

