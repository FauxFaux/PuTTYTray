/*
    Fichier base64.c
    Auteur Bernard Chardonneau
    am�liorations encode64 propos�es par big.lol@free.fr

    Logiciel libre, droits d'utilisation pr�cis�s en fran�ais
    dans le fichier : licence.fr

    Traductions des droits d'utilisation dans les fichiers :
    licence.de , licence.en , licence.es , licence.it
    licence.nl , licence.pt , licence.eo , licence.eo-utf


    Biblioth�que de fonctions permettant d'encoder et de
    d�coder le contenu d'un tableau en base64.
*/

#include "base64.h"


/* encode base64 nbcar caract�res m�moris�s
   dans orig et met le r�sultat dans dest */

void encode64 (char *orig, char *dest, int nbcar)
{
    // groupe de 3 octets � convertir en base 64
    unsigned char octet1, octet2, octet3;

    // tableau d'encodage
    // ce tableau est statique pour �viter une allocation
    // m�moire + initialisation � chaque appel de la fonction
    static char   valcar [] =
          "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";


    // tant qu'il reste au moins 3 caract�res � encoder
    while (nbcar >= 3)
    {
        // extraire 3 caract�res de la chaine et les
        // m�moriser sous la forme d'octets (non sign�s)
        octet1 = *(orig++);
        octet2 = *(orig++);
        octet3 = *(orig++);

        // d�composer les 3 octets en tranches de 6 bits et les
        // remplacer par les caract�res correspondants dans valcar
        *(dest++) = valcar [octet1 >> 2];
        *(dest++) = valcar [((octet1 & 3) << 4) | (octet2 >> 4)];
        *(dest++) = valcar [((octet2 & 0x0F) << 2) | (octet3 >> 6)];
        *(dest++) = valcar [octet3 & 0x3F];

        // 3 caract�res de moins � traiter
        nbcar -= 3;
    }

    // s'il reste des caract�res � encoder
    if (nbcar)
    {
        // encodage des 6 bits de poids fort du premier caract�re
        octet1 = *(orig++);
        *(dest++) = valcar [octet1 >> 2];

        // s'il ne reste que ce caract�re � encoder
        if (nbcar == 1)
        {
            // encodage des 2 bits de poids faible de ce ce caract�re
            *(dest++) = valcar [(octet1 & 3) << 4];

            // indique qu'aucun autre caract�re n'est encod�
            *(dest++) = '=';
        }
        // sinon (il reste 2 caract�res � encoder)
        else
        {
            // 2 bits de poids faible du 1er caract�re + encodage de l'autre
            octet2 = *orig;
            *(dest++) = valcar [((octet1 & 3) << 4) | (octet2 >> 4)];
            *(dest++) = valcar [(octet2 & 0x0F) << 2];
        }

        // indique qu'aucun autre caract�re n'est encod�
        *(dest++) = '=';
    }

    // fin de l'encodage
    *dest = '\0';
}



/* d�code le contenu de buffer encod� base64, met le r�sultat
   dans buffer et retourne le nombre de caract�res convertis */

int decode64 (char *buffer)
{
    int  car;        // caract�re du fichier
    char valcar [4]; // valeur apr�s conversion des caract�res
    int  i;          // compteur
    int  posorig;    // position dans la ligne pass�e en param�tre
    int  posdest;    // position dans la nouvelle ligne g�n�r�e


    // initialisations
    posorig = 0;
    posdest = 0;

    // tant que non fin de ligne
    while (buffer [posorig] > ' ' && buffer [posorig] != '=')
    {
        // d�coder la valeur de 4 caract�res
        for (i = 0; i < 4 && buffer [posorig] != '='; i++)
        {
            // r�cup�rer un caract�re dans la ligne
            car = buffer [posorig++];

            // d�coder ce caract�re
            if ('A' <= car && car <= 'Z')
                valcar [i] = car - 'A';
            else if ('a' <= car && car <= 'z')
                valcar [i] = car + 26 - 'a';
            else if ('0' <= car && car <= '9')
                valcar [i] = car + 52 - '0';
            else if (car == '+')
                valcar [i] = 62;
            else if (car == '/')
                valcar [i] = 63;
        }

        // recopier les caract�res correspondants dans le buffer
        buffer [posdest++] = (valcar [0] << 2) | (valcar [1] >> 4);

        // sauf si indicateur de fin de message
        if (i > 2)
        {
            buffer [posdest++] = (valcar [1] << 4) | (valcar [2] >> 2);

            if (i > 3)
                buffer [posdest++] = (valcar [2] << 6) | (valcar [3]);
        }
    }

    // terminer le buffer
    buffer [posdest] = '\0';

    // et retourner le nombre de caract�res obtenus
    return (posdest);
}