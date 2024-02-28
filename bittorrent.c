#include <mpi.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRACKER_RANK 0
#define MAX_FILES 10
#define MAX_FILENAME 15
#define HASH_SIZE 32
#define MAX_CHUNKS 100

/*
 * Structura pentru swarm-ul unui fisier din tracker
 */
struct swarm_file {
    int clients[10];
    int seed[10];
    int nr_hashes[10];
    char hashes[10][MAX_CHUNKS][HASH_SIZE + 1]; 
};

/*
 * Structura pentru informatiile detinute de tracker
 */
struct tracker_list{
    int nr_files;
    char files[MAX_FILES][MAX_FILENAME + 1];
    struct swarm_file swarm[MAX_FILES];
};

/*
 * Structura pentru un fisier detinut de client
 */
struct file {
    char name_file[MAX_FILENAME + 1];
    int nr_chunks;
    char hashes[MAX_CHUNKS][HASH_SIZE + 1];
};

/*
 * Structura pentru informatiile detinute de client
 */
struct client {
    int rank;
    int nr_owned_files;
    struct file owned_files[MAX_FILES];
    int nr_wanted_files;
    char wanted_files[MAX_FILES][MAX_FILENAME + 1];
    struct file wanted[MAX_FILES];
};

/*
 * Structura pentru informatiile primite de la tracker
 */
struct from_tracker {
    char file[MAX_FILENAME + 1];
    int nr_hashes;
    char hashes[MAX_CHUNKS][HASH_SIZE + 1];
    int client;
};

/*
 * Functie prin care se trimit la tracker
 * fisierele initiale detinute de client
 */
void send_data_to_tracker(struct client *customer) {
    MPI_Send(&(customer->nr_owned_files), 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);

    for (int i = 0; i < customer->nr_owned_files; i++) {
        MPI_Send(customer->owned_files[i].name_file, strlen(customer->owned_files[i].name_file) + 1, 
                    MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
        MPI_Send(&(customer->owned_files[i].nr_chunks), 1, MPI_INT, TRACKER_RANK, 0, MPI_COMM_WORLD);
        
        for (int j = 0; j < customer->owned_files[i].nr_chunks; j++) {
            MPI_Send(customer->owned_files[i].hashes[j], HASH_SIZE + 1, MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD);
       }
    }
}

/*
 * Functie prin care tracker-ul primeste datele trimise
 * de client si le stocheaza
 */
void initial_data_tracker(struct tracker_list *tracker, int nr, int source, int location) {
    MPI_Status status;
    int tag;
    /*
     * Se alege tag-ul corespunzator
     */
    if (location == 2)
        tag = source + 23;
    else tag = 0;

    for (int i = 0; i < nr; i++) {
        char filename[MAX_FILENAME + 1];
        MPI_Recv(filename, MAX_FILENAME + 1, MPI_CHAR, source, tag, MPI_COMM_WORLD, &status);
        
        /*
         * Se verifica daca fisierul exista deja in lista din tracker
         */
        int index_file = -1;
        for (int j = 0; j < tracker->nr_files; j++) {
            if (strcmp(tracker->files[j], filename) == 0) {
                index_file = j;
                break;
            }
        }

        /*
         * Daca nu, il adaugam
         */
        if (index_file == -1) {
            index_file = tracker->nr_files;
            tracker->nr_files++;
            strcpy(tracker->files[index_file], filename);
        }
    
        /*
         * Pentru fisierul de la index_file,
         * adaug datele primite la indexul source-1 
         * (rank-ul clientului de la care provin)
         */
        int nr_chunks;
        MPI_Recv(&tracker->swarm[index_file].nr_hashes[source - 1], 1, MPI_INT, 
                    source, tag, MPI_COMM_WORLD, &status);
        tracker->swarm[index_file].seed[source - 1] = 1;
        tracker->swarm[index_file].clients[source - 1] = 1;
        for (int j = 0; j < tracker->swarm[index_file].nr_hashes[source - 1]; j++) {
            MPI_Recv(tracker->swarm[index_file].hashes[source - 1][j], HASH_SIZE + 1, MPI_CHAR, 
                        source, tag, MPI_COMM_WORLD, &status);
        }
    }
}

/*
 * Functie prin care se solicita informatii de la tracker
 * despre fisierele dorite de client
 */
void request_list_from_tracker(struct client *customer, int location) {
    int tag;
    if (location == 2) 
        tag = customer->rank + 33;
    else tag = 0;

    MPI_Send(&(customer->nr_wanted_files), 1, MPI_INT, TRACKER_RANK, tag, MPI_COMM_WORLD);
    for (int i = 0; i < customer->nr_wanted_files; i++)
        MPI_Send(customer->wanted_files[i], strlen(customer->wanted_files[i]) + 1, MPI_CHAR, 
                    TRACKER_RANK, tag, MPI_COMM_WORLD);
}

/*
 * Functie prin care tracker-ul trimite clientului
 * informatiile cerute
 */
void send_list_to_client(struct tracker_list *tracker, int source, int numtasks, int location) {
    int nr_wanted;
    MPI_Status status;
    int tag;
    if (location == 2)
        tag = source + 33;
    else tag = 0;

    /* 
     * Se primesc numarul de fisiere si numele fisierelor pe care 
     * clientul cu rank-ul source le vrea
     */
    MPI_Recv(&nr_wanted, 1, MPI_INT, source, tag, MPI_COMM_WORLD, &status);
    char wanted[nr_wanted][MAX_FILENAME + 1];
    for (int j = 0; j < nr_wanted; j++)
        MPI_Recv(wanted[j], MAX_FILENAME + 1, MPI_CHAR, source, tag, MPI_COMM_WORLD, &status);

    int nr_clients[nr_wanted]; 
    for (int j = 0; j < nr_wanted; j++)
        nr_clients[j] = 0;

    /*
     * Se numara cati clienti detin fiecare fisier cautat
     */
    for (int j = 0; j < tracker->nr_files; j++) {
        for (int k = 0; k < nr_wanted; k++) {
            if (strcmp(tracker->files[j], wanted[k]) == 0) {
                for (int o = 0; o < numtasks - 1; o++) {
                    if (tracker->swarm[j].clients[o] == 1) {
                        nr_clients[k]++;
                    }
                }
            }
        }
    }

    for (int j = 0; j < nr_wanted; j++)
        MPI_Send(&nr_clients[j], 1, MPI_INT, source, tag, MPI_COMM_WORLD);

    /*
     * Se trimit datele corespunzatoare fiecarui fisier dorit
     */
    for (int j = 0; j < tracker->nr_files; j++) {
        for (int k = 0; k < nr_wanted; k++) {
            if (strcmp(tracker->files[j], wanted[k]) == 0) {
                for (int o = 0; o < numtasks - 1; o++) {
                    if (tracker->swarm[j].clients[o] == 1) {
                        MPI_Send(wanted[k], strlen(wanted[k]) + 1, MPI_CHAR, source, tag, MPI_COMM_WORLD);
                        MPI_Send(&(tracker->swarm[j].nr_hashes[o]), 1, MPI_INT, source, tag, MPI_COMM_WORLD);
                        
                        for (int f = 0; f < tracker->swarm[j].nr_hashes[o]; f++)
                            MPI_Send(tracker->swarm[j].hashes[o][f], HASH_SIZE + 1, MPI_CHAR, 
                                        source, tag, MPI_COMM_WORLD);
                        
                        int rank = o + 1;
                        MPI_Send(&rank, 1, MPI_INT, source, tag, MPI_COMM_WORLD);
                    }
                }
            }
        }
    }
}

/*
 * Functie prin care se salveaza datele primite de la tracker 
 */
void get_list_from_tracker(int *nr_clients, struct from_tracker info[10][10], 
                                            struct client *customer, int location) {
    MPI_Status status;
    int tag;
    if (location == 2)
        tag = customer->rank + 33;
    else tag = 0;

    for (int i = 0; i < customer->nr_wanted_files; i++)
        MPI_Recv(&nr_clients[i], 1, MPI_INT, TRACKER_RANK, tag, MPI_COMM_WORLD, &status);

    /*
     * Variabila folosita pentru a retine
     * datele primite de la tracker
     */
    struct from_tracker info2[10][10];
    for (int i = 0; i < customer->nr_wanted_files; i++) {
        for (int j = 0; j < nr_clients[i]; j++) {
            MPI_Recv(info2[i][j].file, MAX_FILENAME + 1, MPI_CHAR, TRACKER_RANK, tag, MPI_COMM_WORLD, &status);
            MPI_Recv(&(info2[i][j].nr_hashes), 1, MPI_INT, TRACKER_RANK, tag, MPI_COMM_WORLD, &status);
            
            for (int k = 0; k < info2[i][j].nr_hashes; k++)
                MPI_Recv(info2[i][j].hashes[k], HASH_SIZE + 1, MPI_CHAR, 
                            TRACKER_RANK, tag, MPI_COMM_WORLD, &status);
            
            MPI_Recv(&(info2[i][j].client), 1, MPI_INT, TRACKER_RANK, tag, MPI_COMM_WORLD, &status);
        }
    }

    /*
     * Se face ordonarea datelor primite astfel incat
     * la indexul i sa se gaseasca datele despre
     * fisierul i cautat (wanted[i])
     */
    int m = 0, n = 0;
    for (int i = 0; i < customer->nr_wanted_files; i++) {
        for (int j = 0; j < customer->nr_wanted_files; j++) {
            for (int l = 0; l < nr_clients[j]; l++) {
                if (strcmp(info2[j][l].file, customer->wanted_files[i]) == 0) {
                    strcpy(info[i][n].file, customer->wanted_files[i]);
                    info[i][n].nr_hashes = info2[j][l].nr_hashes;
                    info[i][n].client = info2[j][l].client;
                    for (int k = 0; k < info2[j][l].nr_hashes; k++) {
                        strcpy(info[i][n].hashes[k], info2[j][l].hashes[k]);
                    }
                }
            }
        }
            
    }
}

/*
 * Functie prin care se trimit la tracker
 * noile fisiere/segmente primite
 */
void send_new_data(struct client *customer, int nr) {
    MPI_Send(&nr, 1, MPI_INT, TRACKER_RANK, customer->rank + 23, MPI_COMM_WORLD);

    for (int i = 0; i < nr; i++) {
        MPI_Send(customer->wanted[i].name_file, strlen(customer->wanted[i].name_file) + 1, 
                            MPI_CHAR, TRACKER_RANK, customer->rank + 23, MPI_COMM_WORLD);
        MPI_Send(&(customer->wanted[i].nr_chunks), 1, MPI_INT, TRACKER_RANK, customer->rank + 23, MPI_COMM_WORLD);
        
        for (int j = 0; j < customer->wanted[i].nr_chunks; j++)
            MPI_Send(customer->wanted[i].hashes[j], HASH_SIZE + 1, MPI_CHAR, 
                        TRACKER_RANK, customer->rank + 23, MPI_COMM_WORLD);
    }
}

void *download_thread_func(void *arg)
{
    MPI_Status status;
    struct client *customer = (struct client*)arg;
    
    /*
     * Initial, se trimite la tracker cerere pentru
     * a obtine lista cu clientii care au fisierele
     * dorite, impreuna cu hash-urile acestora
     */
    char ask[5] = "LIST";
    MPI_Send(ask, 5, MPI_CHAR, TRACKER_RANK, 90, MPI_COMM_WORLD);
    request_list_from_tracker(customer, 1);
    
    /* 
     * Se salveaza numele, hash urile si clientii 
     * care au informatii despre fisiere
     */
    struct from_tracker info[10][10];
    int nr_clients[customer->nr_wanted_files];
    get_list_from_tracker(nr_clients, info, customer, 1);

    /*
     * Variabila folosita pentru a putea afla momentul
     * terminarii descarcarii tuturor fisierelor clientului
     */
    int finish[customer->nr_wanted_files];
    for (int i = 0; i < customer->nr_wanted_files; i++)
        finish[i] = 0;
    
    /*
     * Pentru fiecare fisier pe care il vreau
     */
    for (int i = 0; i < customer->nr_wanted_files; i++) {
        while (1) {
            int max_hashes;
            int random;
            /*
             * Caut in lista de fisiere primita de la tracker
             */
            for (int p = 0; p < customer->nr_wanted_files; p++) {
                if (strcmp(customer->wanted_files[i], info[p][0].file) == 0) {
                    /*
                     * Daca fisierul nu exista in lista wanted, se pune
                     */
                    if (customer->wanted[i].nr_chunks == 0) {
                        strcpy(customer->wanted[i].name_file, info[p][0].file);
                        
                        /*
                         * Se gaseste dimensiunea maxima posibila a fisierului curent
                         */
                        max_hashes = info[p][0].nr_hashes;
                        for (int d = 0; d < nr_clients[p]; d++) {
                            if (info[p][d].nr_hashes > max_hashes) {
                                max_hashes = info[p][d].nr_hashes;
                                random = d;
                            }
                        }
                    }
                    
                    /*
                     * Daca exista mai multi clienti care detin fisierul,
                     * se alege random unul dintre ei
                     */
                    if (nr_clients[p] > 1) {
                        random = rand() % nr_clients[p];
                        while (strcmp(info[p][0].file, info[p][random].file) != 0 ||
                                customer->wanted[i].nr_chunks > info[p][random].nr_hashes)
                            random = rand() % nr_clients[p];
                    }

                    int nr = customer->wanted[i].nr_chunks;
                    int ten_points = 0;
                    int pick_another = 0;
                    
                    for (int j = nr; j < max_hashes; j++) {
                        /* 
                         * Se trimite cerere pentru hash-ul urmator dorit
                         */
                        MPI_Send(info[p][random].hashes[customer->wanted[i].nr_chunks], HASH_SIZE + 1, 
                            MPI_CHAR, info[p][random].client, info[p][random].client, MPI_COMM_WORLD);
                        
                        /*
                         * Se primeste confirmarea
                         */
                        int reply;
                        MPI_Recv(&reply, 1, MPI_INT, info[p][random].client, 
                                info[p][random].client, MPI_COMM_WORLD, &status);

                        /*
                         * Se salveaza hash-ul
                         */
                        strcpy(customer->wanted[i].hashes[customer->wanted[i].nr_chunks], 
                                info[p][random].hashes[customer->wanted[i].nr_chunks]);
                        customer->wanted[i].nr_chunks++;

                        ten_points++;
                        
                        /*
                         * La fiecare 10 hash-uri primite
                         * se trimite update catre tracker cu fisierele si segmentele detinute
                         * si se asteapta noi date despre fisierele dorite
                         */
                        if (ten_points % 10 == 0 || customer->wanted[i].nr_chunks == max_hashes) {
                            char not[5] = "UPDT";
                            MPI_Send(not, 5, MPI_CHAR, TRACKER_RANK, 90, MPI_COMM_WORLD);
                        
                            send_new_data(customer, i);
                            request_list_from_tracker(customer, 2);
                            get_list_from_tracker(nr_clients, info, customer, 2);
                            
                            /*
                             * De asemenea, se incearca gasirea altui client de la care sa descarc segmente
                             */
                            pick_another = 1;
                            break;
                        }
                    }
                    if (pick_another == 0) break;
                }
            }
            /*
             * Daca s-a terminat de descarcat tot fisierul, 
             * se trimite mesaj de notificare catre tracker,
             * se salveaza fisierul intr-un fisier de iesire
             * si se trece la urmatorul fisier dorit
             */
            if (customer->wanted[i].nr_chunks == max_hashes) {
               char semn[] = "FILE";
               MPI_Send(&semn, strlen(semn) + 1, MPI_CHAR, TRACKER_RANK, 90, MPI_COMM_WORLD);
               MPI_Send(&customer->wanted_files[i], strlen(customer->wanted_files[i]) + 1, 
                            MPI_CHAR, TRACKER_RANK, 100, MPI_COMM_WORLD);

                char file[MAX_FILENAME + 1];
                sprintf(file, "client%d_%s", customer->rank, customer->wanted_files[i]);
                
                FILE* f = fopen(file, "w");
                for (int j = 0; j < customer->wanted[i].nr_chunks - 1; j++) {
                    fprintf(f, customer->wanted[i].hashes[j]);
                    fprintf(f, "\n");
                }
                
                fprintf(f, customer->wanted[i].hashes[customer->wanted[i].nr_chunks - 1]);
                fclose(f);
                break;
            }
        }
    }

    /*
     * Cand se termina de descarcat toate fisierele
     * se notifica tracker-ul
     */
    char bn[] = "GATA";
    MPI_Send(&bn, strlen(bn) + 1, MPI_CHAR, TRACKER_RANK, 90, MPI_COMM_WORLD);

    return NULL;
}

void *upload_thread_func(void *arg)
{
    MPI_Request req;
    struct client *customer = (struct client*)arg;
    MPI_Status status;

    int run = 1;
    while (run) {
        /*
         * Se asteapta primirea unui hash
         */
        char hash[HASH_SIZE + 1];
        MPI_Recv(hash, HASH_SIZE + 1, MPI_CHAR, MPI_ANY_SOURCE, customer->rank, MPI_COMM_WORLD, &status);
    
        /*
         * Daca se primeste mesaj de la tracker anuntand
         * finalizarea tuturor descarcarilor, se opreste functia
         */
        if (strcmp(hash, "aaaaajjjjjuuuutttttoooooorrrrrrr") == 0) {
            break;
        }
        
        /*
         * Altfel, se trimite confirmare
         */
        int succ = 1;
        MPI_Send(&succ, 1, MPI_INT, status.MPI_SOURCE, customer->rank, MPI_COMM_WORLD);
    }

    return NULL;
}

void tracker(int numtasks, int rank) {
    MPI_Status status;
    struct tracker_list *tracker = (struct tracker_list*)malloc(sizeof(struct tracker_list));
    tracker->nr_files = 0;
    
    /*
     * Variabila folosita pentru a contoriza
     * cati clienti nu au de terminat descarcarea
     */
    int terminated[numtasks];
    for (int i = 0; i < numtasks;i++) {
        terminated[i] = 0;
    }
    /*
     * De la fiecare client se primesc fisierele 
     * pe care le detine si hash-urile aferente
     */
    for (int i = 0; i < numtasks - 1; i++) {
        int nr;
        MPI_Recv(&nr, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
        int source = status.MPI_SOURCE;
        initial_data_tracker(tracker, nr, source, 1);
    }   

    /*
     * Se trimite mesaj catre toti clientii ca pot sa continue,
     * tracker-ul are toate datele de inceput
     */
    char ack[] = "ACK";
    int run = 1;
    for (int i = 1; i <= numtasks - 1; i++) {
        MPI_Send(ack, strlen(ack) + 1, MPI_CHAR, i, 0, MPI_COMM_WORLD);
    }

    while (1) {
        /*
         * Se asteapta primirea unui mesaj de la orice client
         */
        char req[5];
        MPI_Recv(req, 5, MPI_CHAR, MPI_ANY_SOURCE, 90, MPI_COMM_WORLD, &status);
        int source = status.MPI_SOURCE;

        /*
         * Daca mesajul este o cerere, tracker-ul 
         * ii trimite clientului datele cerute
         */
        if (strcmp(req, "LIST") == 0) {
            send_list_to_client(tracker, source, numtasks, 1);
        }

        /*
         * Daca mesajul este un update, tracker-ul
         * isi actualizeaza datele si ii trimite clientului
         * ceea ce cauta
         */
        if (strcmp(req, "UPDT") == 0) {
            int nr;
            MPI_Recv(&nr, 1, MPI_INT, source, source + 23, MPI_COMM_WORLD, &status);
            initial_data_tracker(tracker, nr, source, 2);
            send_list_to_client(tracker, source, numtasks, 2);
        }

        /*
         * Daca mesajul este o confirmare de terminare
         * de descarcare a unui fisier, tracker-ul marcheaza
         * clientul respectiv ca seed pentru fisier
         */
        if (strcmp(req, "FILE") == 0) {
            char file[MAX_FILENAME + 1];
            MPI_Recv(&file, MAX_FILENAME + 1, MPI_CHAR, source, 100, MPI_COMM_WORLD, &status);
            for (int i = 0; i<tracker->nr_files; i++) {
                if (strcmp(tracker->files[i], file) == 0) {
                    tracker->swarm[i].seed[source - 1] = 1;
                    break;
                }
            }
            
        }
    
        /*
         * Daca mesajul este o confirmare de terminare
         * de descarcare a tuturor fisierelor, tracker-ul
         * retine acest lucru in variabila specifica
         */
        if (strcmp(req, "GATA") == 0) {
            terminated[source - 1] = 1;
        }

        /*
         * Se verifica daca toti clientii au terminat de descarcat
         * Daca da, se opreste while -ul
         */
        int all_done = 1;
        for (int i = 0; i < numtasks - 1; i++)
            if (terminated[i] == 0)
                all_done = 0;
        if (all_done == 1)
            break;
    }
    
    /*
     * In acest moment toti clientii au terminat de descarcat
     * Se trimite un mesaj pentru inchiderea firul de executie de upload
     */
    for (int i = 0; i < numtasks - 1; i++) {
        char h[HASH_SIZE + 1] = "aaaaajjjjjuuuutttttoooooorrrrrrr";
        MPI_Send(&h, HASH_SIZE + 1, MPI_CHAR, i + 1, i + 1, MPI_COMM_WORLD);
    }
}

void peer(int numtasks, int rank) {
    pthread_t download_thread;
    pthread_t upload_thread;
    void *status;
    MPI_Status status_mpi;
    int r;

    /*
     * Pentru fiecare client se creeaza structura aferenta
     */
    struct client *customer = (struct client*)malloc(sizeof(struct client));
    customer->rank = rank;

    char name_file[MAX_FILENAME + 1];
    sprintf(name_file, "in%d.txt", rank);
    
    /*
     * Se citesc datele din fisier si salveaza corespunzator
     */
    FILE* file = fopen(name_file, "r");
    if (file == NULL) {
        printf("file doesn't exists");
    }

    fscanf(file, "%d", &(customer->nr_owned_files));

    for (int i = 0; i < customer->nr_owned_files; i++) {
        fscanf(file, "%s %d", customer->owned_files[i].name_file, &(customer->owned_files[i].nr_chunks));
        for (int j = 0; j < customer->owned_files[i].nr_chunks; j++) {
            fscanf(file, "%s", customer->owned_files[i].hashes[j]);
        }  
    }
   
    fscanf(file, "%d", &(customer->nr_wanted_files));
    for (int i = 0; i < customer->nr_wanted_files; i++) {
        fscanf(file, "%s", customer->wanted_files[i]);
    }
    
    /*
     * Se trimit datele initiale catre tracker
     */
    send_data_to_tracker(customer);

    /*
     * Se primeste de la tracker confirmarea ca pot sa continui
    */
    char response[4];
    MPI_Recv(response, sizeof(response), MPI_CHAR, TRACKER_RANK, 0, MPI_COMM_WORLD, &status_mpi);

    if (strcmp(response, "ACK") == 0) {
        r = pthread_create(&download_thread, NULL, download_thread_func, (void *) customer);
        if (r) {
            printf("Eroare la crearea thread-ului de download\n");
            exit(-1);
        }

        r = pthread_create(&upload_thread, NULL, upload_thread_func, (void *) customer);
        if (r) {
            printf("Eroare la crearea thread-ului de upload\n");
            exit(-1);
        }
       
        r = pthread_join(download_thread, &status);
        if (r) {
            printf("Eroare la asteptarea thread-ului de download\n");
            exit(-1);
        }

        r = pthread_join(upload_thread, &status);
        if (r) {
            printf("Eroare la asteptarea thread-ului de upload\n");
            exit(-1);

        }
        
    }
}
 
int main (int argc, char *argv[]) {
    int numtasks, rank;
    
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    if (provided < MPI_THREAD_MULTIPLE) {
        fprintf(stderr, "MPI nu are suport pentru multi-threading\n");
        exit(-1);
    }
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    if (rank == TRACKER_RANK) {
        tracker(numtasks, rank);
    } else {
        peer(numtasks, rank);
    }

    MPI_Finalize();
}
