/* iosrc.h - source d'octets adressable (ISO sur disque, ou image en RAM) */
#ifndef XIB_IOSRC_H
#define XIB_IOSRC_H

#include <stdint.h>
#include <stddef.h>

typedef struct iosrc iosrc;

/* Ouvre un fichier en lecture seule (offset 64 bits). */
iosrc *iosrc_open(const char *path);
/* Cree une source a partir d'un buffer memoire (pratique pour les tests). */
iosrc *iosrc_from_memory(const void *data, uint64_t size);
/* Lecture a offset absolu. Retourne 0 si OK, -1 sinon. */
int iosrc_read_at(iosrc *s, uint64_t off, void *buf, size_t len);
uint64_t iosrc_size(iosrc *s);
void iosrc_close(iosrc *s);

#endif /* XIB_IOSRC_H */
