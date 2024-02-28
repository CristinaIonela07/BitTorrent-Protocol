# BitTorrent-Protocol

Scopul acestui proiect este simularea protocolului BitTorrent de partajare
peer-to-peer de fisiere, folosind MPI.

Am folosit structuri diferite: o structura pentru tracker si toate datele 
care se salveaza acolo, una pentru client, cu datele initiale citite din 
fisier si lista lui de dorinte, si una speciala pentru datele primite de 
client de la tracker.

struct tracker_list: structura folosita in tracker
        nr_files = numarul total de fisiere 
        files[] = numele fisierelor
        swarm_file[] = swarm-ul fiecarui fisier

struct swarm_file: structura folosita a exemplifica toate datele unui fisier
                    din tracker
        clients[i] = are valaorea 1 daca clientul cu rank-ul i detine parti din
                    fisierul curent
        seed[i] = are valaorea 1 daca clientul cu rank-ul i este seed pentru fisier
        nr_hashes[i] = numarul de hash-uri pe care clientul i il are din fisier
        hashes[i][j] = al j-lea hash detinut de clientul i

struct file: structura folosita pentru datele fisierelor
    name_file = numele fisierului
    nr_chunks = numarul de hash-uri
    hashes[i] = hash-urile efective in ordine

struct client: structura folosita pentru clienti
        rank = rank-ul clientului
        nr_owned_files = numarul de fisiere detinute de client initial
        owned_files = fisierele detinute initial de client
        nr_wanted_files = numarul de fisiere pe care clientul de vrea
        wanted_files = numele fisierelor pe care clientul de vrea
        wanted[i] = toate datele necesare pentru fisierul i dorit

struct from_tracker: structura folosita pentru datele primite de la tracker
        file = numele fisierului
        nr_hashes = numarul de hash-uri
        hashes = hash-urile efective
        client = rank-ul cleintului care detine segmentul

Am creat numeroase functii ajutatoare, pentru a face codul mai usor de inteles
si de urmarit, atat pentru tracker, cat si pentru client.

void send_data_to_tracker: trimit catre tracker datele initiale obtinute din
    fisierul de intrare: numarul de fisiere detinute, numele lor si hash-urile.

void initial_data_tracker: primesc datele de la clientul cu rank-ul source
    si le salvez in structura definita special pentru tracker. Verific in fisierele
    pe care le detin deja daca numele fisierului primit exista, caz in care
    retin indexul la care se gaseste (index_file). Daca nu, il adaug in lista
    de fisiere. Pentru fisierul de la index_file, adaug datele primite la indexul
    source-1 (source -1 reprezinta rank-ul clientului de la care provin).

void request_list_from_tracker: functia este folosita de clienti pentru a trimite
    tracker-ului numele fisierelor despre care vor sa afle mai multe informatii
    (numele fisierelor dorite).

void send_list_to_client: functia este folosita de tracker; acesta raspunde cererii
    clientului si incepe sa numere cati clienti detin fiecare fisier dorit. 
    Apoi trimite acest numar, urmat de toate datele pe care le are despre cine
    il detine si cate segmente detine. Stim ca clientul j detine fisierul i daca
    tracker->swarm[i].clients[j] == 1.

void get_list_from_tracker: se salveaza datele primite de la tracker intr-o structura
    info[i][j], unde i reprezinta indexul la care se gaseste numele fisierului in
    wanted_files([i]), iar j reprezinta un client cu hash-urile corespunzatoare.
    Am observat ca datele nu se primesc in ordine, mai exact ca la indexul i nu se
    gaseste intotdeauna fisierul corespunzator, motiv pentru care a fost nevoie
    de o sortare si reasezare a datelor in structura.

void send_new_data: functia este folosita pentru a trimite la tracker fisierele 
    "primite", fara a mai retrimite si fisierele initiale.

In multe functii se observa folosirea unei variabile, location, care in functie de
valoare, modifica tag-ul functiilor de MPI_Send si MPI_Recv. Dupa cum am mentionat
anterior, avand probleme din cauza tag-urilor, am decis sa le diversific cat mai
mult, astfel ca la initializare (atat pentru client cat si pentru tracker) folosesc
tag-ul 0, urmand ca in momentul in care clientul doreste sa isi updateze datele
in tracker sa folosesc tag-ul rank+23, iar cand tracker-ul doreste sa ii 
retrimita date clientului, sa folosesc tag-ul rank+33.
De asemenea, folosesc tag-ul 90 pentru a trimite mesaje de notificare catre tracker,
si tag-ul egal cu rank-ul pentru trimiterea mesajelor intre clienti (in upload).

void *download_thread_func: prima si prima data cer si primesc de la tracker
    lista cu clientii care au segmente din fisierele pe care clientul le vrea.
    Apoi, pentru fiecare fisier dorit, incep si caut in lista primita de la
    tracker clienti de la care sa cer segmente. La fiecare 10 segmente obtinute,
    informez tracker-ul despre fisierele pe care le-am obtinut de la inceput si
    pana in prezent si caut alt client de la care sa cer segmente.
    Cand am terminat de descarcat toate hash-urile unui fisier, informez tracker-ul
    despre aceasta si creez fisierul de iesire aferent.
    Cand am terminat de descarcat toate segmentele pentru toate fisierele, informez
    tracker-ul si astfel inchei download-ul.

void *upload_thread_func: pana cand va primi mesaj de oprire de la tracker 
    ("aaaaajjjjjuuuutttttoooooorrrrrrr"), clientul asteapta sa primeasca cereri
    de la clienti pentru hash-uri. Conform cerintei, atunci cand primeste o cerere,
    trimite inapoi un mesaj de confirmare (succ = 1).

void tracker: mai intai se stocheaza datele primite de la toti clientii cu ajutorul
    structurii special create struct tracker_list. Cand a primit toate datele de la
    toti clientii, tracker-ul trimite un mesaj catre fiecare, semn ca isi pot
    continua activitatea (procesele de upload si download). Tracker-ul asteapta
    constant sa primeasca notificari, si, in functie de mesajul primit, executa
    comenzile necesare. Continua sa ruleze pana cand toti clientii termina de
    descarcat fisierele pe care le doresc, moment in care tracker-ul ii anunta
    pe toti ca pot inchide firul de upload, urmand sa se inchida si el.

void peer: se salveaza pentru inceput datele din fisierul de intrare aferent
    clientului cu rank-ul rank. Salvarea datelor se face prin analiza fisierului
    si stocarea lor cu ajutorul structurii struct_client. Urmarorul pas este
    trimiterea tuturor datelor obtinute catre tracker si asteptarea confirmarii
    ca putem merge mai departe. Odata cu mesajul de confirmare, se pot porni
    cele doua firele de executie.
