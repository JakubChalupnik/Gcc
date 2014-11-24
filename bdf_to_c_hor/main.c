#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char byte;
typedef unsigned int word;
typedef unsigned long int dword;


typedef struct {
    int ascii;
    int x, y;   // size
    int width;
    char name [32];
    unsigned char bin [256];
} char_t;

char_t Chars [256];

void DisplayBin (FILE *f, byte b) {
    int i;

    for (i = 0; i < 8; i++) {
        fprintf (f, "%c", (b & (0x80 >> i)) ? '#' : '.');
    }
}

int FindInFile (const char *str, FILE *f, char *line) {
    char *p;

    while (!feof (f)) {
        fgets(line, 1024, f);
        if ((p = strstr (line, str)) != NULL) {
            if (p == line) return 1;
        }
    }

    return 0;
}

void WriteCharComment (FILE *f, char_t *c, int First, int Last) {
    int x, y;

    fprintf (f, "// %c %2.2X '%s'\n", c->ascii, c->ascii, c->name);

    for (y = First; y <= Last; y++) {
        fprintf (f, "// ");
        for (x = 0; x < c->x; x++) {
            DisplayBin (f, c->bin [y * c->x + x]);
        }
        fprintf (f, "\n");
    }
}

void WriteChar (FILE *f, char_t *c, int First, int Last) {
    int x, y;

    for (y = First; y <= Last; y++) {
        for (x = 0; x < c->x; x++) {
            fprintf (f, "0x%2.2X, ", c->bin [y * c->x + x]);
        }
    }

    fprintf (f, "\n");
}

int ProcessChar (FILE *f, char_t *c) {
    char line [1024];
    int i, bitmap;

    if (!FindInFile ("STARTCHAR", f, line)) {
        return 0;
    }

    strcpy (c->name, line + 10);
    c->name [strlen (c->name) - 1] = 0;

    if (!FindInFile ("ENCODING", f, line)) {
        return 0;
    }

    c -> ascii = strtol (line + 9, NULL, 10);

    if (!FindInFile ("DWIDTH", f, line)) {
        return 0;
    }

    c -> width = strtol (line + 7, NULL, 10);

    if (!FindInFile ("BITMAP", f, line)) {
        return 0;
    }

    i = 0;
    while (strstr (line, "ENDCHAR") == NULL) {
        fgets (line, sizeof (line), f);
        bitmap = strtol (line, NULL, 16);
        if (c->width > 7) {
            c->bin [i] = bitmap >> 8;
            i++;
        }
        c->bin [i] = bitmap & 0xFF;
        i++;
    }

    if (c->width > 7) {
        c->x = 2;
        c->y = (i - 1) / 2;
    } else {
        c->x = 1;
        c->y = i - 1;
    }

    return 1;
}

int main (int Argc, char **Argv) {
    FILE *In, *Out;
    int FirstCharacter, LastCharacter;
    char Line[1024];
    char *p;
    char FontName [256];
    unsigned int FontBoundingX, FontBoundingY, FontDescent, FontAscent;
    char_t Character;
    int i;
    int x, y;
    char_t *c;
    int FirstRow, LastRow;
    unsigned char Bitmap;
    char FontFileName[256], OutFileName[256];
    char FontCName [256];

    memset (Chars, 256, sizeof (char_t));

    //
    // Check input parameters
    //

    if (Argc != 5) {
        printf ("usage: bdf2c fontfile font_c_name firstchar lastchar\n");
        return 1;
    }

    //
    // Open font file and process its details
    //

    strcpy (FontFileName, Argv [1]);
    strcat (FontFileName, ".bdf");

    strcpy (OutFileName, Argv [1]);
    strcat (OutFileName, ".h");

    strcpy (FontCName, Argv [2]);

    In = fopen (FontFileName, "rb");
    if (In == NULL) {
        printf ("Cannot open font file\n");
        return 2;
    }

    fseek (In, 0, SEEK_SET);

    if (FindInFile ("FONT ", In, Line)) {
        strcpy (FontName, Line + 5);
    }

    if (FindInFile ("FONTBOUNDINGBOX", In, Line)) {
        FontBoundingX = strtol (Line + 16, &p, 10);
        FontBoundingY = strtol (p, &p, 10);
    }

    if (FindInFile ("FONT_DESCENT", In, Line)) {
        FontDescent = strtol (Line + 12, &p, 10);
    }

    if (FindInFile ("FONT_ASCENT", In, Line)) {
        FontAscent = strtol (Line + 11, &p, 10);
    }

    fseek (In, 0, SEEK_SET);



    //
    // Read first and last character from commandline
    //

    FirstCharacter = strtol (Argv [3], NULL, 10);
    if (FirstCharacter < 0) {
        printf ("Wrong first character: %d\n", FirstCharacter);
        return 6;
    }

    LastCharacter = strtol (Argv [4], NULL, 10);
    if (LastCharacter < 0) {
        printf ("Wrong last character: %d\n", LastCharacter);
        return 6;
    }

    if (LastCharacter < FirstCharacter) {
        printf ("Last character < first character\n");
        return 6;
    }

    //
    // Open output file
    //

    Out = fopen (OutFileName, "wt");
    if (Out == NULL) {
        printf("Cannot open output file\n");
        return 1;
    }

    while (ProcessChar (In, &Character)) {
        if ((Character.ascii >= FirstCharacter) &&
            (Character.ascii <= LastCharacter)) {
                memcpy (&Chars[Character.ascii], &Character, sizeof (char_t));
            }
    }

    //
    // Find the first and last row that's not used by any character.
    // Just go through all of them and create a sticky bitmap.
    //

    memset (&Character, 0, sizeof (char_t));

    for (i = FirstCharacter; i < LastCharacter; i++) {

        c = &Chars [i];
        if (Character.x < c->x) {
            Character.x = c->x;
        }

        if (Character.y < c->y) {
            Character.y = c->y;
        }

        for (y = 0; y < c->y; y++) {
            for (x = 0; x < c->x; x++) {
                Character.bin [y * c->x + x] |= c->bin [y * c->x + x];
            }
        }
    }

    fprintf (Out, "//  Usage map:\n");
    WriteCharComment (Out, &Character, 0, Character.y);

    for (y = 0; y < c->y; y++) {
        Bitmap = 0;
        for (x = 0; x < c->x; x++) {
            Bitmap |= Character.bin [y * c->x + x];
        }

        if (Bitmap != 0) {
            FirstRow = y;
            break;
        }
    }

    for (y = c->y - 1; y >= 0; y--) {
        Bitmap = 0;
        for (x = 0; x < c->x; x++) {
            Bitmap |= Character.bin [y * c->x + x];
        }

        if (Bitmap != 0) {
            LastRow = y;
            break;
        }
    }

    fprintf (Out, "//  Used rows: %d..%d\n", FirstRow, LastRow);
    fprintf (Out, "//  Character size: %d\n", (LastRow - FirstRow + 1) * c->x);

    fprintf (Out, "#define %s_CHARSIZE %d\n", FontCName, (LastRow - FirstRow + 1) * c->x);
    fprintf (Out, "#define %s_FIRSTCHAR %d\n", FontCName, FirstCharacter);
    fprintf (Out, "#define %s_LASTCHAR %d\n", FontCName, LastCharacter);
    fprintf (Out, "#define %s_HEIGHT %d\n", FontCName, LastRow - FirstRow + 1);


    for (i = FirstCharacter; i < LastCharacter; i++) {
        WriteCharComment (Out, &Chars[i], FirstRow, LastRow);
        WriteChar (Out, &Chars[i], FirstRow, LastRow);
    }

    fclose (Out);

    return 0;
}

