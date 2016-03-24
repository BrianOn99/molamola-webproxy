struct parser *new_parser(int sockfd);
void parser_init_global();
void parser_free(struct parser *p);
void parser_reset(struct parser *p);
int parse_request(struct parser *req);
int parse_response(struct parser *reply);
char ** header_to_value(struct parser *req, char field_name[]);
size_t ptr_strlen(char *str[]);
