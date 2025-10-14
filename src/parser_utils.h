#pragma once

#define UNEXPECTED_TOKEN(TOKEN)                                                                  \
printf("ERROR: Unexpected token ");                                                              \
printToken(TOKEN);                                                                               \
printf("\nerror called from\nfile: %s\nfunction: %s\nline: %d\n", __FILE__, __func__, __LINE__); \
exit(1);
