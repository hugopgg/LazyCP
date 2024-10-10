#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <fcntl.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/wait.h>
#include "checksum.h"

#define MAX_BLOC SSIZE_MAX

typedef struct Traitement Traitement;
struct Traitement
{
	long int taille_bloc;
	char *destination;
	char **tab_fichiers;
	int nb_fichiers;
};

/**
 * Affiche l'usage attendu et quitte le programme.
 */
void afficher_usage()
{
	fprintf(stderr, "lcp [-b TAILLE] SOURCE... DESTINATION\n");
	exit(EXIT_FAILURE);
}

/*
 * Verifie si le chemin passe en argument est un repertoire.
 */
bool est_un_repertoire(char *chemin)
{
	struct stat sb;
	return stat(chemin, &sb) == 0 && S_ISDIR(sb.st_mode);
}

/*
 * Retourne la taille d'un fichier
 */
int get_taille(char *fichier)
{
	struct stat sb;
	stat(fichier, &sb);
	return sb.st_size;
}

/*
 * Compare un fichier source et destination bloc par bloc (de taille bloc(parametre)).
 * Si le bloc de destination differe de celui de source, il est reecrit. Si le
 * fichier source est de taille moindre que destination, destination est ecrase
 * et reecrit.
 */
void copie_io_selective(char *source, char *destination, long int bloc)
{
	char *buffer_dataS = (char *)malloc(bloc * sizeof(char));
	char *buffer_dataD = (char *)malloc(bloc * sizeof(char));
	uint32_t checkS, checkD, checkS_buff; // checkD_buff;
	size_t taille_fichier;
	int sizeS = get_taille(source);
	int sizeD = get_taille(destination);
	int oui = 1, non = 0;
	int reponse;

	if (access(source, R_OK) == 0 && access(destination, R_OK) == 0 && access(destination, W_OK) == 0)
	{
		if (sizeS < sizeD)
		{
			remove(destination);
			int fout = open(destination, O_CREAT | O_RDWR, 0666);
			if (fout < 0)
				perror("Erreur open");
			close(fout);
		}

		size_t luS = 1, luD = 0;
		int offsetS = 0, offsetD = 0;
		int pipe_parent[2];
		int pipe_enfant[2];
		pid_t process;

		if (pipe(pipe_parent) == -1 || pipe(pipe_enfant) == -1)
		{
			perror("Erreur pipe");
			exit(EXIT_FAILURE);
		}

		process = fork();
		if (process < 0)
		{
			perror("Erreur fork");
			exit(EXIT_FAILURE);
		}
		else if (process > 0) // parent
		{
			close(pipe_enfant[1]);
			close(pipe_parent[0]);

			int f_in = open(source, O_RDONLY);
			if (f_in < 0)
				perror("Erreur open");

			write(pipe_parent[1], &taille_fichier, sizeof(taille_fichier));

			while (luS > 0)
			{
				luS = read(f_in, buffer_dataS, bloc);
				if (luS > 0)
				{
					write(pipe_parent[1], &luS, sizeof(luS));
					checkS = fletcher32((uint16_t *)buffer_dataS, (size_t)luS);
					write(pipe_parent[1], &checkS, sizeof(checkS));
					read(pipe_enfant[0], &reponse, sizeof(reponse));
					if (reponse == oui)
					{
						write(pipe_parent[1], buffer_dataS, luS);
					}
				}
				else
				{
					luS = 0;
					write(pipe_parent[1], &luS, sizeof(luS));
				}
			}
			wait(NULL);
			close(pipe_parent[1]);
			close(pipe_enfant[0]);
			close(f_in);
		}
		else // enfant
		{
			close(pipe_parent[1]);
			close(pipe_enfant[0]);
			int fin = 0;
			int f_out = open(destination, O_CREAT | O_RDWR, 0666);
			if (f_out < 0)
				perror("Erreur open");

			read(pipe_parent[0], &taille_fichier, sizeof(taille_fichier));

			while (fin == 0)
			{
				read(pipe_parent[0], &luS, sizeof(luS));
				if (luS == 0)
				{
					fin = 1;
				}
				else
				{
					read(pipe_parent[0], &checkS_buff, sizeof(uint32_t));
					offsetD = lseek(f_out, offsetS, SEEK_SET);

					if (offsetD == -1)
					{
						write(pipe_enfant[1], &oui, sizeof(oui));
						read(pipe_parent[0], buffer_dataS, luS);

						write(f_out, buffer_dataS, luS);
					}
					else
					{
						luD = read(f_out, buffer_dataD, luS);
						checkD = fletcher32((uint16_t *)buffer_dataD, (size_t)luD);
						if (checkD != checkS_buff)
						{
							write(pipe_enfant[1], &oui, sizeof(oui));
							read(pipe_parent[0], buffer_dataS, luS);
							offsetD = lseek(f_out, offsetS, SEEK_SET);
							write(f_out, buffer_dataS, luS);
						}
						else
						{
							write(pipe_enfant[1], &non, sizeof(non));
						}
					}
					offsetS = offsetS + luS;
				}
			}
			close(f_out);
			close(pipe_parent[0]);
			close(pipe_enfant[1]);
			exit(0);
		}
	}
	else
	{
		perror("Vous n'avez pas les droits nécéssaire");
		exit(EXIT_FAILURE);
	}

	free(buffer_dataD);
	free(buffer_dataS);
}

/**
 * Complete le chemin si necessaire (+ '/' et filename).
 */
char *set_destination(char *source, char *destination) // const?
{
	char *full_chemin;
	char *nom_fichier;
	char s[PATH_MAX];
	char d[PATH_MAX];
	strcpy(s, source);
	strcpy(d, destination);

	if (access(d, W_OK) == 0)
	{
		nom_fichier = basename(s);
		if (d[strlen(d) - 1] == '/')
		{
			full_chemin = d;
			strcat(full_chemin, nom_fichier);
		}
		else
		{
			char c = '/';
			strncat(d, &c, 1);
			full_chemin = d;
			strcat(full_chemin, nom_fichier);
		}
	}
	else
	{
		fprintf(stderr, "Vous n'avez pas les droits d'écriture sur %s: ", destination);
		perror("");
		exit(EXIT_FAILURE);
	}

	return full_chemin;
}

/**
 * Copie source dans destination selon si destination est un repertoire ou un
 * fichier.
 */
void copie(long int bloc, char **tab_fichiers, int nbr_fichiers, char *destination)
{
	char *chemin_complet;

	if (est_un_repertoire(destination))
	{
		for (int i = 0; i < nbr_fichiers; i++)
		{
			chemin_complet = set_destination(tab_fichiers[i], destination);
			if (access(chemin_complet, F_OK) == 0)
			{
				copie_io_selective(tab_fichiers[i], chemin_complet, bloc);
			}
			else
			{

				int f_out = open(chemin_complet, O_CREAT | O_RDWR, 0666);
				close(f_out);
				copie_io_selective(tab_fichiers[i], chemin_complet, bloc);
			}
		}
	}
	else
	{
		if (access(destination, F_OK) == 0)
		{
			copie_io_selective(tab_fichiers[0], destination, bloc);
		}
		else
		{
			int f_out = open(destination, O_CREAT | O_RDWR, 0666);
			copie_io_selective(tab_fichiers[0], destination, bloc);
			close(f_out);
		}
	}
}

/**
 * Valide le bloc passe en argument et le transforme en entier.
 * Doit etre un entier pair > 0 et < MAX_bloc.
 */
int valider_bloc(char *bloc)
{
	bool valide = true;
	size_t i = 0;

	while (valide && i < strlen(bloc))
	{
		if (isdigit(bloc[i]) != 0)
		{
			i++;
		}
		else
		{
			valide = false;
		}
	}
	long int bloc_i = atoi(bloc);
	if (valide && bloc_i <= MAX_BLOC && bloc_i > 0 && (bloc_i % 2) == 0)
	{
		return bloc_i;
	}
	else
	{
		fprintf(stderr, "bloc non conforme ayant pour cause une des raisons suivante : \n");
		fprintf(stderr, "Pas un entier\n");
		fprintf(stderr, "negatif ou nul\n");
		fprintf(stderr, "impair\n");
		fprintf(stderr, "Dépasse la capacité du système\n");
		exit(EXIT_FAILURE);
	}
}

/**
 * Valide les sources a copier. Si le ou un des fichier(s) source(s) n'existe
 * pas ou n'est pas un fichier regulier, le programme quite avec un message d'erreur.
 */
char **valider_sources(char **tab_fichiers, int nbr_args, int index_d)
{
	char **t;
	struct stat sb;

	for (int i = index_d; i < nbr_args - 1; i++)
	{

		if (access(tab_fichiers[i], R_OK) != 0 || (stat(tab_fichiers[i], &sb) == 0 && !(S_ISREG(sb.st_mode))))
		{
			fprintf(stderr, "%s n'existe pas ou est inaccessible.\n", tab_fichiers[i]);
			exit(EXIT_FAILURE);
		}
	}

	t = malloc((nbr_args - (index_d + 1)) * sizeof(char *));
	int j = 0;
	for (int i = index_d; i < nbr_args - 1; i++)
	{
		t[j] = malloc(sizeof(char) * strlen(tab_fichiers[i]));
		strcpy(t[j], tab_fichiers[i]);
		j++;
	}

	return t;
}

/**
 * Valide les arguments avec lesquelles le programme a ete lance.
 * Construit un tableau de fichiers sources valides pour faire copie.
 */
Traitement valider_arguments(int nbr_args, char *tab_args[])
{
	int bflag = 0;
	char *bloc;
	Traitement t;

	if (nbr_args < 3)
	{
		afficher_usage();
	}
	else
	{
		int option;
		while ((option = getopt(nbr_args, tab_args, ":b:?")) != -1)
		{
			switch (option)
			{
			case 'b':
				bflag = 1;
				bloc = optarg;
				break;
			case ':':
				afficher_usage();
				break;
			case '?':
				afficher_usage();
				break;
			}
		}

		if (bflag == 1)
		{
			if (nbr_args < 5)
			{
				afficher_usage();
			}
			if (nbr_args > 5 && !est_un_repertoire(tab_args[nbr_args - 1]))
			{
				fprintf(stderr, "La destination n'est pas valide.\n");
				exit(EXIT_FAILURE);
			}
			t.tab_fichiers = valider_sources(tab_args, nbr_args, optind);
			t.taille_bloc = valider_bloc(bloc);
			t.nb_fichiers = nbr_args - 4;
		}
		else
		{
			if (nbr_args > 3 && !est_un_repertoire(tab_args[nbr_args - 1]))
			{
				fprintf(stderr, "La destination n'est pas valide.\n");
				exit(EXIT_FAILURE);
			}
			if (strcmp(tab_args[1], "--") == 0)
			{
				t.tab_fichiers = valider_sources(tab_args, nbr_args, 2);
				t.nb_fichiers = nbr_args - 3;
			}
			else
			{
				t.tab_fichiers = valider_sources(tab_args, nbr_args, 1);
				t.nb_fichiers = nbr_args - 2;
			}

			t.taille_bloc = 32;
		}
		t.destination = tab_args[nbr_args - 1];
	}

	return t;
}

int main(int argc, char *argv[])
{
	Traitement t;
	t = valider_arguments(argc, argv);
	copie(t.taille_bloc, t.tab_fichiers, t.nb_fichiers, t.destination);
	free(t.tab_fichiers);

	return 0;
}