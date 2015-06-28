
/**
 * File:   WordExtractor.c
 * Author: Constantine / https://github.com/jessesilva
 * 
 * Criado dia 27 de junho de 2015.
 * 
 * Utilizado para extrair palavras sem repetições de página web para serem 
 * usadas como dorks em buscador do Bing.
 * 
 * Usuários Linux comentar a linha '#define WINDOWS_USER'.
 * 
 * Windows: gcc -std=c99 WordExtractor.c -o WordExtractor.exe -lws2_32 && WordExtractor.exe
 * Linux: gcc -std=c99 WordExtractor.c -o WordExtractor ; ./WordExtractor
 */

// Deixar linha comentada se for usuário Linux.
#define WINDOWS_USER
#define out printf
#define true 1
#define false 0
#define MAX 256
#ifdef NULL
#define null NULL
#else
#define null (void *)0
#endif

#define zero(PTR,SIZE) memset(PTR, '\0', SIZE)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WINDOWS_USER
    #include <Winsock2.h>
    #include <ws2tcpip.h>  
    #include <Windows.h>
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
#endif

typedef struct {
    unsigned char *protocol;
    unsigned char *domain;
    unsigned char *path;
    unsigned int port;
} url_data_t;

typedef struct {
    unsigned char *buffer;
    unsigned int size;
} http_request_t;

typedef struct {
    unsigned int total_word_found;
    unsigned int total_unique_word;
} status_t;

int startup (const unsigned char *url, const unsigned char *output);
void show_help (const char *argv);
void *xmalloc (const unsigned int size);
url_data_t *format_url (const unsigned char *url);
http_request_t *http_get (url_data_t *param);
status_t *word_extractor (http_request_t *param, const unsigned char *output);
void show_statistics (status_t *status);
unsigned int is_number (const char *string);
void show_banner (const unsigned char *argv);

int main(int argc, char** argv) {
    int result = EXIT_FAILURE;

    if (argc != 3) {
        show_help(argv[0]);
        exit(EXIT_SUCCESS);
    }
    
    #ifdef WINDOWS_USER
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2),&wsa) != 0)
        return EXIT_FAILURE;
    #endif
    
    result = startup(argv[1], argv[2]);
    
    #ifdef WINDOWS_USER
        WSACleanup();
    #endif
    return result;
}

/**
 * Extrai palavras da página e salva em arquivo de saída.
 * 
 * @url     Página Web que será acessada.
 * @output  Nome do arquivo de saída onde será salvo as palavras extraídas.
 * @returns Retorno para função main.
 */
int startup (const unsigned char *url, const unsigned char *output) {
    url_data_t *data = null;
    int result = EXIT_FAILURE;
    
    if ((data = format_url(url)) != null) {
        if (!strstr(data->protocol, "https")) {
            show_banner(data->domain);
            http_request_t *response = null;
            if ((response = http_get(data)) != null) {
                status_t *status = null;
                if ((status = word_extractor(response, output)) != null) {
                    show_statistics(status);
                    free(status);
                    result = EXIT_SUCCESS;
                }
                response->size = 0;
                free(response->buffer);
                free(response);
            }
            data->port = 0;
            free(data->path);
            free(data->domain);
            free(data->protocol);
            free(data);
        } else 
            out("HTTPS not supported, use HTTP.\n");
    }
    
    return result;
}

/**
 * Exibe estatisticas finais.
 * 
 * @param   Estrutura contendo informações que serão exibidas.
 */
void show_statistics (status_t *param) {
    status_t *status = param;
    out(" + Total of %d unique of %d words encountered.",
        status->total_unique_word, status->total_word_found);
    for (int a=0; a<20; a++) out(" ");
    out("\n\n Finished!\n\n");
}

/**
 * Extrai palavras de buffer e salva sem repetições em arquivo de saída.
 * 
 * @param   Estrutura contendo buffer e seu tamanho.
 * @output  Nome do arquivo de saída que serão salvos os dados.
 * @return  Estrutura com informações de estatísticas.
 */
status_t *word_extractor (http_request_t *param, const unsigned char *output) {
    http_request_t *response = param;
    char delemiters [] = " ,-.(){}<>_\"'||:;?![*/\\=\n\t\r&%$#@^~+´`";
    char *ptr = strtok (response->buffer, delemiters);
    FILE *output_handle = null;
    
    if (!param || !output)
        return (status_t *) null; 
        
    status_t *status = (status_t *) xmalloc(sizeof(status_t));
    status->total_unique_word = 0;
    status->total_word_found = 0;
    
    while (ptr != NULL) {
        if (!ptr) {
            ptr = strtok (NULL, delemiters);
            continue;
        }
        if (!(strlen(ptr) > 2)) {
            ptr = strtok (NULL, delemiters);
            continue;
        }
        if (is_number(ptr) == true) {
            ptr = strtok (NULL, delemiters);
            continue;
        }
        
        unsigned char line [MAX];
        unsigned int flag = false;
        zero(line, MAX);
        
        if ((output_handle = fopen(output, "r")) != null) {
            while (fgets(line, MAX, output_handle)) {
                int equal_counter = 0;
                for (int a=0; line[a]!='\0'; a++) {
                    if (line[a] == '\n') break;
                    if (a <= strlen(ptr)) 
                        if (line[a] == ptr[a]) equal_counter++;
                }
                if (equal_counter == strlen(ptr)) {
                    flag = true;
                    break;
                }
            }
            fclose(output_handle);
        }
        
        if (flag == false)
            if ((output_handle = fopen(output, "a+")) != null) {
                fprintf(output_handle, "%s\n", ptr);
                fclose(output_handle);
                status->total_unique_word++;
            }
        
        status->total_word_found++;
        ptr = strtok (NULL, delemiters);
        out(" | Scanning, %d unique of %d words encountered."
            "\r /\r +\r |\r |\r /\r +\r +\r", 
                status->total_unique_word, status->total_word_found);
    }
    
    return status;
}

/**
 * Responsável por enviar requisições do tipo GET para servidor HTTP.
 * 
 * @param   Estrutura com dados da conexão.
 * @return  Dados retornados da página e seu tamanho em estrutura de controle.
 */
http_request_t *http_get (url_data_t *param) {
    url_data_t *data = param;
    struct hostent *host;
    int sock = (-1);
    
    if ((host = gethostbyname(data->domain)) == null)
      return (http_request_t *) null;
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(data->port);
    addr.sin_addr.s_addr = *(u_long *)host->h_addr_list[0];
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        out("socket failed.\n");
        return (http_request_t *) null;
    }
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        out("connect failed.\n");
        return (http_request_t *) null;
    }
    
    unsigned char *header = (unsigned char *) xmalloc((MAX*2)*sizeof(unsigned char));
    http_request_t *response = (http_request_t *) xmalloc(sizeof(http_request_t));
    response->size = 0;
    
    sprintf(header, 
      "GET %s HTTP/1.1\r\n"
      "Host: %s\r\n"
      "User-Agent: Mozilla/5.0 (Windows NT 6.1; rv:38.0) Gecko/20100101 Firefox/38.0\r\n"
      "Connection: close\r\n\r\n", data->path, data->domain);
    
    if (send(sock, header, strlen(header), 0) == ( -1)) {
        out("send failed.\n");
        goto end_connection;
    }
    
    response->buffer = (unsigned char *) xmalloc(MAX*sizeof(unsigned char));
    while (true) {
        int value = recv(sock, &response->buffer[response->size], MAX, 0);
        if (value == 0 || value < 0) break;
        else {
            response->size += value;
            if ((response->buffer = (unsigned char *) realloc(response->buffer, 
                    response->size + (MAX*sizeof(unsigned char)))) == null) {
                out("realloc failed.\n");
                goto free_buffer;
            }
            out(" | Receiving data from %s, total data received: %d."
                "\r /\r +\r |\r |\r /\r +\r +\r",
                    data->domain, response->size);
        }
    }
    
    response->buffer[response->size] = '\0';
    if (response->size > 0 && response->buffer != null)
        if (!strlen(response->buffer) == response->size)
            goto free_buffer;
    
    out(" + Total of %d bytes received from %s.", 
        response->size, data->domain);
    for (int a=0; a<20; a++) out(" ");
    out("\n");
    
    end_connection:
    #ifdef WINDOWS_USER
        closesocket(sock);
    #else
        close(sock);
    #endif

    return response;
    
    free_buffer:
    free(response->buffer);
    free(response);
        
    return (http_request_t *) null;
}

/**
 * Formata URL e insere em estrutura.
 * 
 * @url     URL a ser parseada.
 * @retuns  Estrutura com partes da URL formatada. 
 */
url_data_t *format_url (const unsigned char *url) {
    if (!url)
        return (url_data_t *) null;
    
    url_data_t *data = (url_data_t *) xmalloc(sizeof(url_data_t));
    data->protocol = (unsigned char *) xmalloc(9*sizeof(unsigned char));
    data->domain = null;
    data->path = null;
    data->port = 80;
    
    unsigned char *strings [] = {
        "http://", "https://", ":", "http", "://", "/", null
    };
    
    if (strstr(url, strings[1]))
        memcpy(data->protocol, strings[1], strlen(strings[1]));
    else
        memcpy(data->protocol, strings[0], strlen(strings[0]));
    
    char *ptr = null;
    unsigned char temporary [MAX];
    zero(temporary, MAX);
    int port_flag = false;
    
    // Extrai port.
    if (strstr(url, strings[4])) {
        if (ptr = strstr(url, strings[4]))
            if ((ptr = strstr(++ptr, strings[2])) != null)
                port_flag = true;
            else ptr = null;
    } else {
        if ((ptr = strstr(url, strings[2])) != null)
            port_flag = true;
        else ptr = null;
    }
    
    if (port_flag == true && ptr != null) {
        char *old = ptr++;
        unsigned int size = 1;
        while (++ptr) {
            if (ptr[0] == '/' || ptr[0] == '\0') break;
            size++;
        }
        memcpy(temporary, ++old, size);
        temporary[size] = '\0';
        data->port = (int) strtol(temporary, NULL, 10);
    }
    
    // Extrai domain.
    zero(temporary, MAX);
    for (int a=strlen(url); a>0 ; a--)
        if (url[a] == '.') {
            for (int b=0; ; b++) {
                if (url[a+b+1] == '/' || url[a+b+1] == ':' || url[a+b+1] == '\0') {
                    int x = strlen(url) - a;
                    x = (strlen(url) - x) + b + 1;
                    memcpy(temporary, url, x);
                    ptr = null;
                    if (strstr(temporary, strings[0])) {
                        ptr = strstr(url, strings[0]);
                        ptr += strlen(strings[0]);
                        x -= strlen(strings[0]);
                    } else if (strstr(temporary, strings[1])) {
                        ptr = strstr(url, strings[1]);
                        ptr += strlen(strings[1]);
                        x -= strlen(strings[1]);
                    }
                    data->domain = (unsigned char *) xmalloc((x+1)*sizeof(unsigned char));
                    if (ptr != null) 
                        memcpy(data->domain, ptr, x);
                    else
                        memcpy(data->domain, url, x); 
                    break;
                }
            }
        }
    
    // Extrai path.
    for (int a=strlen(url); a>0 ; a--)
        if (url[a] == '.') {
            for (int b=0,c=0; url[a+b+1]!='\0'; b++) {
                if (url[a+b+1] == '/') {
                    for (c=0; url[a+b+c+1]!='\0'; c++)
                       temporary[c] = url[a+b+c+1];
                    temporary[c] = '\0';
                    data->path = (unsigned char *) 
                            xmalloc((strlen(temporary)*sizeof(unsigned char))+1);
                    memcpy(data->path, temporary, strlen(temporary));
                    break;
                }
            } break;
        }
    
    if (data->path == null) {
        data->path = (unsigned char *) 
            xmalloc((strlen(strings[5])*sizeof(unsigned char))+1);
        memcpy(data->path, strings[5], strlen(strings[5]));
    }
    /*
    out(">%s\n", data->path);
    out(">%s\n", data->domain);
    out(">%d\n", data->port);
    */
    if (data->domain != null && data->path != null && data->protocol != null) 
        return data;
    return (url_data_t *) null;
}

/**
 * malloc com tratamento para preencher buffer com zeros.
 * 
 * @size    Tamanho dos dados a ser alocado.
 * @returns Ponteiro para região alocada.
 */
void *xmalloc (const unsigned int size) {
    if (size) {
        void *ptr = null;
        if ((ptr = malloc(size)) != null) {
            memset(ptr, 0, size);
            return ptr;
        }
    }
    return null;
}

/**
 * Verifica se string contém apenas números decimais.
 * 
 * @string  String que será verificada.
 * @returns Se conter apenas números retorna 'true', caso contrário 'false'.
 */
unsigned int is_number (const char *string) {
    if (string) {
        int counter = 0;
        for (int a=0; string[a]!='\0'; a++)
            if (string[a] >= '0' && string[a] <= '9')
                counter++;
        if (counter == strlen(string))
            return true;
    }
    return false;
}

/**
 * Exibe help.
 */
void show_help (const char *argv) {
    out("\n Word Extractor - Coded by Constantine.\n\n");
    out("  Use: %s URL OUTPUT-FILE.TXT\n", argv);
    out("  Ex.: %s http://host.com/page.php?id=7 output.txt\n\n", argv);
}

/**
 * Exibe banner.
 */
void show_banner (const unsigned char *argv) {
    out("\n Word Extractor - v1.0\n Target: %s\n\n", argv);
}

/* EOF. */
