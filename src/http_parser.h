struct parser *new_parser(int sockfd);
void parser_free(struct parser *req);
int parse_request(struct parser *req);
char *header_to_value(struct parser *req, char field_name[]);
