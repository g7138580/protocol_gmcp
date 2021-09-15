/******************************************************************************
 Protocol snippet by KaVir.  Released into the public domain in February 2011.

 In case this is not legally possible:

 The copyright holder grants any entity the right to use this work for any 
 purpose, without any conditions, unless such conditions are required by law.
 ******************************************************************************/

/******************************************************************************
 This snippet was originally designed to be codebase independent, but has been 
 modified slightly so that it runs out-of-the-box on Merc derivatives.  To use 
 it for other codebases, just change the code in the "Diku/Merc" section below.
 ******************************************************************************/

/******************************************************************************
 Header files.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/telnet.h>
#include <time.h>
#include <malloc.h>
#include <alloca.h>
#include <ctype.h>

#include "protocol.h"

/******************************************************************************
 The following section is for Diku/Merc derivatives.  Replace as needed.
 ******************************************************************************/

#include "merc.h"

static void Write( descriptor_t *apDescriptor, const char *apData )
{
   if ( apDescriptor != NULL && !apDescriptor->fcommand )
   {
      if ( apDescriptor->pProtocol->WriteOOB > 0 || 
         apDescriptor->outtop == 0 )
      {
         apDescriptor->pProtocol->WriteOOB = 2;
      }
   }
   write_to_buffer( apDescriptor, apData, 0 );
}

static void ReportBug( const char *apText )
{
   bug( apText, 0 );
}

static void InfoMessage( descriptor_t *apDescriptor, const char *apData )
{
   Write( apDescriptor, "\t[F210][\toINFO\t[F210]]\tn " );
   Write( apDescriptor, apData );
}

static void CompressStart( descriptor_t *apDescriptor )
{
   /* If your mud uses MCCP (Mud Client Compression Protocol), you need to 
    * call whatever function normally starts compression from here - the 
    * ReportBug() call should then be deleted.
    * 
    * Otherwise you can just ignore this function.
    */
   ReportBug( "CompressStart() in protocol.c is being called, but it doesn't do anything!\n" );
}

static void CompressEnd( descriptor_t *apDescriptor )
{
   /* If your mud uses MCCP (Mud Client Compression Protocol), you need to 
    * call whatever function normally starts compression from here - the 
    * ReportBug() call should then be deleted.
    * 
    * Otherwise you can just ignore this function.
    */
   ReportBug( "CompressEnd() in protocol.c is being called, but it doesn't do anything!\n" );
}

/*************** START GMCP ***************/
static void ParseGMCP( descriptor_t *apDescriptor, char *string );
static char *OneArg( char *fStr, char *bStr );
static char *GMCPStrip( char *text );
static void WriteGMCP( descriptor_t *apDescriptor, GMCP_PACKAGE package );
static jsmntok_t *jsmn_alloc_token( jsmn_parser *parser, jsmntok_t *tokens, const size_t num_tokens );
static void jsmn_fill_token( jsmntok_t *token, const jsmntype_t type, const int start, const int end );
static int jsmn_parse_primitive( jsmn_parser *parser, const char *js, const size_t len, jsmntok_t *tokens, const size_t num_tokens );
static int jsmn_parse_string( jsmn_parser *parser, const char *js, const size_t len, jsmntok_t *tokens, const size_t num_tokens );
int jsmn_parse( jsmn_parser *parser, const char *js, const size_t len, jsmntok_t *tokens, const unsigned int num_tokens );
void jsmn_init( jsmn_parser *parser );
char *PullJSONString( int start, int end, const char *string );
/*************** END GMCP ***************/

/******************************************************************************
 MSDP file-scope variables.
 ******************************************************************************/

/* These are for the GUI_VARIABLES, my unofficial extension of MSDP.  They're 
 * intended for clients that wish to offer a generic GUI - not as nice as a 
 * custom GUI, admittedly, but still better than a plain terminal window.
 *
 * These are per-player so that they can be customised for different characters 
 * (eg changing 'mana' to 'blood' for vampires).  You could even allow players 
 * to customise the buttons and gauges themselves if you wish.
 */
static const char s_Button1[] = "\005\002Help\002help\006";
static const char s_Button2[] = "\005\002Look\002look\006";
static const char s_Button3[] = "\005\002Score\002help\006";
static const char s_Button4[] = "\005\002Equipment\002equipment\006";
static const char s_Button5[] = "\005\002Inventory\002inventory\006";

static const char s_Gauge1[]  = "\005\002Health\002red\002HEALTH\002HEALTH_MAX\006";
static const char s_Gauge2[]  = "\005\002Mana\002blue\002MANA\002MANA_MAX\006";
static const char s_Gauge3[]  = "\005\002Movement\002green\002MOVEMENT\002MOVEMENT_MAX\006";
static const char s_Gauge4[]  = "\005\002Exp TNL\002yellow\002EXPERIENCE\002EXPERIENCE_MAX\006";
static const char s_Gauge5[]  = "\005\002Opponent\002darkred\002OPPONENT_HEALTH\002OPPONENT_HEALTH_MAX\006";

/******************************************************************************
 MSDP variable table.
 ******************************************************************************/

/* Macros for readability, but you can remove them if you don't like them */
#define NUMBER_READ_ONLY           false, false, false, false, -1, -1,  0, NULL
#define NUMBER_READ_ONLY_SET_TO(x) false, false, false, false, -1, -1,  x, NULL
#define STRING_READ_ONLY           true,  false, false, false, -1, -1,  0, NULL
#define NUMBER_IN_THE_RANGE(x,y)   false, true,  false, false,  x,  y,  0, NULL
#define BOOLEAN_SET_TO(x)          false, true,  false, false,  0,  1,  x, NULL
#define STRING_WITH_LENGTH_OF(x,y) true,  true,  false, false,  x,  y,  0, NULL
#define STRING_WRITE_ONCE(x,y)     true,  true,  true,  false, -1, -1,  0, NULL
#define STRING_GUI(x)              true,  false, false, true,  -1, -1,  0, x

static variable_name_t VariableNameTable[eMSDP_MAX+1] = 
{
   /* General */
   { eMSDP_CHARACTER_NAME,   "CHARACTER_NAME",   STRING_READ_ONLY }, 
   { eMSDP_SERVER_ID,        "SERVER_ID",        STRING_READ_ONLY }, 
   { eMSDP_SERVER_TIME,      "SERVER_TIME",      NUMBER_READ_ONLY }, 
   { eMSDP_SNIPPET_VERSION,  "SNIPPET_VERSION",  NUMBER_READ_ONLY_SET_TO(SNIPPET_VERSION) }, 

   /* Character */
   { eMSDP_AFFECTS,          "AFFECTS",          STRING_READ_ONLY }, 
   { eMSDP_ALIGNMENT,        "ALIGNMENT",        NUMBER_READ_ONLY }, 
   { eMSDP_EXPERIENCE,       "EXPERIENCE",       NUMBER_READ_ONLY }, 
   { eMSDP_EXPERIENCE_MAX,   "EXPERIENCE_MAX",   NUMBER_READ_ONLY }, 
   { eMSDP_EXPERIENCE_TNL,   "EXPERIENCE_TNL",   NUMBER_READ_ONLY }, 
   { eMSDP_HEALTH,           "HEALTH",           NUMBER_READ_ONLY }, 
   { eMSDP_HEALTH_MAX,       "HEALTH_MAX",       NUMBER_READ_ONLY }, 
   { eMSDP_LEVEL,            "LEVEL",            NUMBER_READ_ONLY }, 
   { eMSDP_RACE,             "RACE",             STRING_READ_ONLY }, 
   { eMSDP_CLASS,            "CLASS",            STRING_READ_ONLY }, 
   { eMSDP_MANA,             "MANA",             NUMBER_READ_ONLY }, 
   { eMSDP_MANA_MAX,         "MANA_MAX",         NUMBER_READ_ONLY }, 
   { eMSDP_WIMPY,            "WIMPY",            NUMBER_READ_ONLY }, 
   { eMSDP_PRACTICE,         "PRACTICE",         NUMBER_READ_ONLY }, 
   { eMSDP_MONEY,            "MONEY",            NUMBER_READ_ONLY }, 
   { eMSDP_MOVEMENT,         "MOVEMENT",         NUMBER_READ_ONLY }, 
   { eMSDP_MOVEMENT_MAX,     "MOVEMENT_MAX",     NUMBER_READ_ONLY }, 
   { eMSDP_HITROLL,          "HITROLL",          NUMBER_READ_ONLY }, 
   { eMSDP_DAMROLL,          "DAMROLL",          NUMBER_READ_ONLY }, 
   { eMSDP_AC,               "AC",               NUMBER_READ_ONLY }, 
   { eMSDP_STR,              "STR",              NUMBER_READ_ONLY }, 
   { eMSDP_INT,              "INT",              NUMBER_READ_ONLY }, 
   { eMSDP_WIS,              "WIS",              NUMBER_READ_ONLY }, 
   { eMSDP_DEX,              "DEX",              NUMBER_READ_ONLY }, 
   { eMSDP_CON,              "CON",              NUMBER_READ_ONLY }, 
   { eMSDP_STR_PERM,         "STR_PERM",         NUMBER_READ_ONLY }, 
   { eMSDP_INT_PERM,         "INT_PERM",         NUMBER_READ_ONLY }, 
   { eMSDP_WIS_PERM,         "WIS_PERM",         NUMBER_READ_ONLY }, 
   { eMSDP_DEX_PERM,         "DEX_PERM",         NUMBER_READ_ONLY }, 
   { eMSDP_CON_PERM,         "CON_PERM",         NUMBER_READ_ONLY }, 

   /* Combat */
   { eMSDP_OPPONENT_HEALTH,  "OPPONENT_HEALTH",  NUMBER_READ_ONLY }, 
   { eMSDP_OPPONENT_HEALTH_MAX,"OPPONENT_HEALTH_MAX",NUMBER_READ_ONLY }, 
   { eMSDP_OPPONENT_LEVEL,   "OPPONENT_LEVEL",   NUMBER_READ_ONLY }, 
   { eMSDP_OPPONENT_NAME,    "OPPONENT_NAME",    STRING_READ_ONLY }, 

   /* World */
   { eMSDP_AREA_NAME,        "AREA_NAME",        STRING_READ_ONLY }, 
   { eMSDP_ROOM_EXITS,       "ROOM_EXITS",       STRING_READ_ONLY }, 
   { eMSDP_ROOM_NAME,        "ROOM_NAME",        STRING_READ_ONLY }, 
   { eMSDP_ROOM_VNUM,        "ROOM_VNUM",        NUMBER_READ_ONLY }, 
   { eMSDP_WORLD_TIME,       "WORLD_TIME",       NUMBER_READ_ONLY }, 

   /* Configurable variables */
   { eMSDP_CLIENT_ID,        "CLIENT_ID",        STRING_WRITE_ONCE(1,40) }, 
   { eMSDP_CLIENT_VERSION,   "CLIENT_VERSION",   STRING_WRITE_ONCE(1,40) }, 
   { eMSDP_PLUGIN_ID,        "PLUGIN_ID",        STRING_WITH_LENGTH_OF(1,40) }, 
   { eMSDP_ANSI_COLORS,      "ANSI_COLORS",      BOOLEAN_SET_TO(1) }, 
   { eMSDP_XTERM_256_COLORS, "XTERM_256_COLORS", BOOLEAN_SET_TO(0) }, 
   { eMSDP_UTF_8,            "UTF_8",            BOOLEAN_SET_TO(0) }, 
   { eMSDP_SOUND,            "SOUND",            BOOLEAN_SET_TO(0) }, 
   { eMSDP_MXP,              "MXP",              BOOLEAN_SET_TO(0) }, 

   /* GUI variables */
   { eMSDP_BUTTON_1,         "BUTTON_1",         STRING_GUI(s_Button1) }, 
   { eMSDP_BUTTON_2,         "BUTTON_2",         STRING_GUI(s_Button2) }, 
   { eMSDP_BUTTON_3,         "BUTTON_3",         STRING_GUI(s_Button3) }, 
   { eMSDP_BUTTON_4,         "BUTTON_4",         STRING_GUI(s_Button4) }, 
   { eMSDP_BUTTON_5,         "BUTTON_5",         STRING_GUI(s_Button5) }, 
   { eMSDP_GAUGE_1,          "GAUGE_1",          STRING_GUI(s_Gauge1) }, 
   { eMSDP_GAUGE_2,          "GAUGE_2",          STRING_GUI(s_Gauge2) }, 
   { eMSDP_GAUGE_3,          "GAUGE_3",          STRING_GUI(s_Gauge3) }, 
   { eMSDP_GAUGE_4,          "GAUGE_4",          STRING_GUI(s_Gauge4) }, 
   { eMSDP_GAUGE_5,          "GAUGE_5",          STRING_GUI(s_Gauge5) }, 

   { eMSDP_MAX,              "", 0 } /* This must always be last. */
};

/******************************************************************************
 MSSP file-scope variables.
 ******************************************************************************/

static int    s_Players = 0;
static time_t s_Uptime  = 0;

/******************************************************************************
 Local function prototypes.
 ******************************************************************************/

static void Negotiate               ( descriptor_t *apDescriptor );
static void PerformHandshake        ( descriptor_t *apDescriptor, char aCmd, char aProtocol );
static void PerformSubnegotiation   ( descriptor_t *apDescriptor, char aCmd, char *apData, int aSize );
static void SendNegotiationSequence ( descriptor_t *apDescriptor, char aCmd, char aProtocol );
static bool_t ConfirmNegotiation    ( descriptor_t *apDescriptor, negotiated_t aProtocol, bool_t abWillDo, bool_t abSendReply );

static void ParseMSDP               ( descriptor_t *apDescriptor, const char *apData );
static void ExecuteMSDPPair         ( descriptor_t *apDescriptor, const char *apVariable, const char *apValue );

static void ParseATCP               ( descriptor_t *apDescriptor, const char *apData );
#ifdef MUDLET_PACKAGE
static void SendATCP                ( descriptor_t *apDescriptor, const char *apVariable, const char *apValue );
#endif /* MUDLET_PACKAGE */

static void SendMSSP                ( descriptor_t *apDescriptor );

static char *GetMxpTag              ( const char *apTag, const char *apText );

static const char *GetAnsiColour    ( bool_t abBackground, int aRed, int aGreen, int aBlue );
static const char *GetRGBColour     ( bool_t abBackground, int aRed, int aGreen, int aBlue );
static bool_t IsValidColour         ( const char *apArgument );

static bool_t MatchString           ( const char *apFirst, const char *apSecond );
static bool_t PrefixString          ( const char *apPart, const char *apWhole );
static bool_t IsNumber              ( const char *apString );
static char  *AllocString           ( const char *apString );

/******************************************************************************
 ANSI colour codes.
 ******************************************************************************/

static const char s_Clean       [] = "\033[0;00m"; /* Remove colour */

static const char s_DarkBlack   [] = "\033[0;30m"; /* Black foreground */
static const char s_DarkRed     [] = "\033[0;31m"; /* Red foreground */
static const char s_DarkGreen   [] = "\033[0;32m"; /* Green foreground */
static const char s_DarkYellow  [] = "\033[0;33m"; /* Yellow foreground */
static const char s_DarkBlue    [] = "\033[0;34m"; /* Blue foreground */
static const char s_DarkMagenta [] = "\033[0;35m"; /* Magenta foreground */
static const char s_DarkCyan    [] = "\033[0;36m"; /* Cyan foreground */
static const char s_DarkWhite   [] = "\033[0;37m"; /* White foreground */

static const char s_BoldBlack   [] = "\033[1;30m"; /* Grey foreground */
static const char s_BoldRed     [] = "\033[1;31m"; /* Bright red foreground */
static const char s_BoldGreen   [] = "\033[1;32m"; /* Bright green foreground */
static const char s_BoldYellow  [] = "\033[1;33m"; /* Bright yellow foreground */
static const char s_BoldBlue    [] = "\033[1;34m"; /* Bright blue foreground */
static const char s_BoldMagenta [] = "\033[1;35m"; /* Bright magenta foreground */
static const char s_BoldCyan    [] = "\033[1;36m"; /* Bright cyan foreground */
static const char s_BoldWhite   [] = "\033[1;37m"; /* Bright white foreground */

static const char s_BackBlack   [] = "\033[1;40m"; /* Black background */
static const char s_BackRed     [] = "\033[1;41m"; /* Red background */
static const char s_BackGreen   [] = "\033[1;42m"; /* Green background */
static const char s_BackYellow  [] = "\033[1;43m"; /* Yellow background */
static const char s_BackBlue    [] = "\033[1;44m"; /* Blue background */
static const char s_BackMagenta [] = "\033[1;45m"; /* Magenta background */
static const char s_BackCyan    [] = "\033[1;46m"; /* Cyan background */
static const char s_BackWhite   [] = "\033[1;47m"; /* White background */

/******************************************************************************
 Protocol global functions.
 ******************************************************************************/

protocol_t *ProtocolCreate( void )
{
   int i; /* Loop counter */
   protocol_t *pProtocol;

   /* Called the first time we enter - make sure the table is correct */
   static bool_t bInit = false;
   if ( !bInit )
   {
      bInit = true;
      for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
      {
         if ( VariableNameTable[i].Variable != i )
         {
            ReportBug( "MSDP: Variable table does not match the enums in the header.\n" );
            break;
         }
      }
   }

   pProtocol = malloc(sizeof(protocol_t));
   pProtocol->WriteOOB = 0;
   for ( i = eNEGOTIATED_TTYPE; i < eNEGOTIATED_MAX; ++i )
      pProtocol->Negotiated[i] = false;
   pProtocol->bIACMode = false;
   pProtocol->bNegotiated = false;
   pProtocol->bRenegotiate = false;
   pProtocol->bNeedMXPVersion = false;
   pProtocol->bBlockMXP = false;
   pProtocol->bTTYPE = false;
   pProtocol->bECHO = false;
   pProtocol->bNAWS = false;
   pProtocol->bCHARSET = false;
   pProtocol->bMSDP = false;
   pProtocol->bMSSP = false;
   pProtocol->bATCP = false;
   pProtocol->bMSP = false;
   pProtocol->bMXP = false;
   pProtocol->bMCCP = false;
   pProtocol->b256Support = eUNKNOWN;
   pProtocol->ScreenWidth = 0;
   pProtocol->ScreenHeight = 0;
   pProtocol->pMXPVersion = AllocString("Unknown");
   pProtocol->pLastTTYPE = NULL;
   pProtocol->pVariables = malloc(sizeof(MSDP_t*)*eMSDP_MAX);

   for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
   {
      pProtocol->pVariables[i] = malloc(sizeof(MSDP_t));
      pProtocol->pVariables[i]->bReport = false;
      pProtocol->pVariables[i]->bDirty = false;
      pProtocol->pVariables[i]->ValueInt = 0;
      pProtocol->pVariables[i]->pValueString = NULL;

      if ( VariableNameTable[i].bString )
      {
         if ( VariableNameTable[i].pDefault != NULL )
            pProtocol->pVariables[i]->pValueString = AllocString(VariableNameTable[i].pDefault);
         else if ( VariableNameTable[i].bConfigurable )
            pProtocol->pVariables[i]->pValueString = AllocString("Unknown");
         else /* Use an empty string */
            pProtocol->pVariables[i]->pValueString = AllocString("");
      }
      else if ( VariableNameTable[i].Default != 0 )
      {
         pProtocol->pVariables[i]->ValueInt = VariableNameTable[i].Default;
      }
   }

	/*************** START GMCP ***************/
	pProtocol->bGMCP = false;
	pProtocol->bSGA = false;

	for ( i = 0; i < GMCP_SUPPORT_MAX; i++ )
		pProtocol->bGMCPSupport[i] = 0;

	for ( i = 0; i < GMCP_MAX; i++ )
		pProtocol->GMCPVariable[i] = AllocString( "" );

	for ( i = 0; i < GMCP_PACKAGE_MAX; i++ )
		pProtocol->bGMCPUpdatePackage[i] = 0;
	/*************** END GMCP ***************/

   return pProtocol;
}

void ProtocolDestroy( protocol_t *apProtocol )
{
   int i; /* Loop counter */

   for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
   {
      free(apProtocol->pVariables[i]->pValueString);
      free(apProtocol->pVariables[i]);
   }

   free(apProtocol->pVariables);
   free(apProtocol->pLastTTYPE);
   free(apProtocol->pMXPVersion);

	/*************** START GMCP ***************/
	for ( i = 0; i < GMCP_MAX; i++ )
		free( apProtocol->GMCPVariable[i] );
	/*************** END GMCP ***************/

   free(apProtocol);
}

void ProtocolInput( descriptor_t *apDescriptor, char *apData, int aSize, char *apOut )
{
   static char CmdBuf[MAX_PROTOCOL_BUFFER+1];
   static char IacBuf[MAX_PROTOCOL_BUFFER+1];
   int CmdIndex = 0;
   int IacIndex = 0;
   int Index;

   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

   for ( Index = 0; Index < aSize; ++Index )
   {
      /* If we'd overflow the buffer, we just ignore the input */
      if ( CmdIndex >= MAX_PROTOCOL_BUFFER || IacIndex >= MAX_PROTOCOL_BUFFER )
      {
         ReportBug("ProtocolInput: Too much incoming data to store in the buffer.\n");
         return;
      }

      /* IAC IAC is treated as a single value of 255 */
      if ( apData[Index] == (char)IAC && apData[Index+1] == (char)IAC )
      {
         if ( pProtocol->bIACMode )
            IacBuf[IacIndex++] = (char)IAC;
         else /* In-band command */
            CmdBuf[CmdIndex++] = (char)IAC;
         Index++;
      }
      else if ( pProtocol->bIACMode )
      {
         /* End subnegotiation. */
         if ( apData[Index] == (char)IAC && apData[Index+1] == (char)SE )
         {
            Index++;
            pProtocol->bIACMode = false;
            IacBuf[IacIndex] = '\0';
            if ( IacIndex >= 2 )
               PerformSubnegotiation( apDescriptor, IacBuf[0], &IacBuf[1], IacIndex-1 );
            IacIndex = 0;
         }
         else
           IacBuf[IacIndex++] = apData[Index];
      }
      else if ( apData[Index] == (char)27 && apData[Index+1] == '[' && 
         isdigit(apData[Index+2]) && apData[Index+3] == 'z' )
      {
         char MXPBuffer [1024];
         char *pMXPTag = NULL;
         int i = 0; /* Loop counter */

         Index += 4; /* Skip to the start of the MXP sequence. */

         while ( Index < aSize && apData[Index] != '>' && i < 1000 )
         {
            MXPBuffer[i++] = apData[Index++];
         }
         MXPBuffer[i++] = '>';
         MXPBuffer[i] = '\0';

         if ( ( pMXPTag = GetMxpTag( "CLIENT=", MXPBuffer ) ) != NULL )
         {
            /* Overwrite the previous client name - this is harder to fake */
            free(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString);
            pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pMXPTag);
         }

         if ( ( pMXPTag = GetMxpTag( "VERSION=", MXPBuffer ) ) != NULL )
         {
            const char *pClientName = pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString;

            free(pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString);
            pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString = AllocString(pMXPTag);

            if ( MatchString( "MUSHCLIENT", pClientName ) )
            {
               /* MUSHclient 4.02 and later supports 256 colours. */
               if ( strcmp(pMXPTag, "4.02") >= 0 )
               {
                  pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
                  pProtocol->b256Support = eYES;
               }
               else /* We know for sure that 256 colours are not supported */
                  pProtocol->b256Support = eNO;
            }
            else if ( MatchString( "CMUD", pClientName ) )
            {
               /* CMUD 3.04 and later supports 256 colours. */
               if ( strcmp(pMXPTag, "3.04") >= 0 )
               {
                  pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
                  pProtocol->b256Support = eYES;
               }
               else /* We know for sure that 256 colours are not supported */
                  pProtocol->b256Support = eNO;
            }
            else if ( MatchString( "ATLANTIS", pClientName ) )
            {
               /* Atlantis 0.9.9.0 supports XTerm 256 colours, but it doesn't 
                * yet have MXP.  However MXP is planned, so once it responds 
                * to a <VERSION> tag we'll know we can use 256 colours.
                */
               pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
               pProtocol->b256Support = eYES;
            }
         }

         if ( ( pMXPTag = GetMxpTag( "MXP=", MXPBuffer ) ) != NULL )
         {
            free(pProtocol->pMXPVersion);
            pProtocol->pMXPVersion = AllocString(pMXPTag);
         }

         if ( strcmp(pProtocol->pMXPVersion, "Unknown") )
         {
            Write( apDescriptor, "\n" );
            sprintf( MXPBuffer, "MXP version %s detected and enabled.\r\n", 
               pProtocol->pMXPVersion );
            InfoMessage( apDescriptor, MXPBuffer );
         }
      }
      else /* In-band command */
      {
         if ( apData[Index] == (char)IAC )
         {
            switch ( apData[Index+1] )
            {
               case (char)SB: /* Begin subnegotiation. */
                  Index++;
                  pProtocol->bIACMode = true;
                  break;

               case (char)DO: /* Handshake. */
               case (char)DONT:
               case (char)WILL:
               case (char)WONT: 
                  PerformHandshake( apDescriptor, apData[Index+1], apData[Index+2] );
                  Index += 2;
                  break;

               case (char)IAC: /* Two IACs count as one. */
                  CmdBuf[CmdIndex++] = (char)IAC;
                  Index++;
                  break;

               default: /* Skip it. */
                  Index++;
                  break;
            }
         }
         else
            CmdBuf[CmdIndex++] = apData[Index];
      }
   }

   /* Terminate the two buffers */
   IacBuf[IacIndex] = '\0';
   CmdBuf[CmdIndex] = '\0';

   /* Copy the input buffer back to the player. */
   strcat( apOut, CmdBuf );
}

const char *ProtocolOutput( descriptor_t *apDescriptor, const char *apData, int *apLength )
{
   static char Result[MAX_OUTPUT_BUFFER+1];
   const char Tab[] = "\t";
   const char MSP[] = "!!";
   const char MXPStart[] = "\033[1z<";
   const char MXPStop[] = ">\033[7z";
   const char LinkStart[] = "\033[1z<send>\033[7z";
   const char LinkStop[] = "\033[1z</send>\033[7z";
   bool_t bTerminate = false, bUseMXP = false, bUseMSP = false;
#ifdef COLOUR_CHAR
   bool_t bColourOn = COLOUR_ON_BY_DEFAULT;
#endif /* COLOUR_CHAR */
   int i = 0, j = 0; /* Index values */

   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;
   if ( pProtocol == NULL || apData == NULL )
      return apData;

   /* Strip !!SOUND() triggers if they support MSP or are using sound */
   if ( pProtocol->bMSP || pProtocol->pVariables[eMSDP_SOUND]->ValueInt )
      bUseMSP = true;

   for ( ; i < MAX_OUTPUT_BUFFER && apData[j] != '\0' && !bTerminate && 
      (*apLength <= 0 || j < *apLength); ++j )
   {
      if ( apData[j] == '\t' )
      {
         const char *pCopyFrom = NULL;

         switch ( apData[++j] )
         {
            case '\t': /* Two tabs in a row will display an actual tab */
               pCopyFrom = Tab;
               break;
            case 'n':
               pCopyFrom = s_Clean;
               break;
            case 'r': /* dark red */
               pCopyFrom = ColourRGB(apDescriptor, "F200");
               break;
            case 'R': /* light red */
               pCopyFrom = ColourRGB(apDescriptor, "F500");
               break;
            case 'g': /* dark green */
               pCopyFrom = ColourRGB(apDescriptor, "F020");
               break;
            case 'G': /* light green */
               pCopyFrom = ColourRGB(apDescriptor, "F050");
               break;
            case 'y': /* dark yellow */
               pCopyFrom = ColourRGB(apDescriptor, "F220");
               break;
            case 'Y': /* light yellow */
               pCopyFrom = ColourRGB(apDescriptor, "F550");
               break;
            case 'b': /* dark blue */
               pCopyFrom = ColourRGB(apDescriptor, "F002");
               break;
            case 'B': /* light blue */
               pCopyFrom = ColourRGB(apDescriptor, "F005");
               break;
            case 'm': /* dark magenta */
               pCopyFrom = ColourRGB(apDescriptor, "F202");
               break;
            case 'M': /* light magenta */
               pCopyFrom = ColourRGB(apDescriptor, "F505");
               break;
            case 'c': /* dark cyan */
               pCopyFrom = ColourRGB(apDescriptor, "F022");
               break;
            case 'C': /* light cyan */
               pCopyFrom = ColourRGB(apDescriptor, "F055");
               break;
            case 'w': /* dark white */
               pCopyFrom = ColourRGB(apDescriptor, "F222");
               break;
            case 'W': /* light white */
               pCopyFrom = ColourRGB(apDescriptor, "F555");
               break;
            case 'a': /* dark azure */
               pCopyFrom = ColourRGB(apDescriptor, "F014");
               break;
            case 'A': /* light azure */
               pCopyFrom = ColourRGB(apDescriptor, "F025");
               break;
            case 'j': /* dark jade */
               pCopyFrom = ColourRGB(apDescriptor, "F031");
               break;
            case 'J': /* light jade */
               pCopyFrom = ColourRGB(apDescriptor, "F052");
               break;
            case 'l': /* dark lime */
               pCopyFrom = ColourRGB(apDescriptor, "F140");
               break;
            case 'L': /* light lime */
               pCopyFrom = ColourRGB(apDescriptor, "F250");
               break;
            case 'o': /* dark orange */
               pCopyFrom = ColourRGB(apDescriptor, "F520");
               break;
            case 'O': /* light orange */
               pCopyFrom = ColourRGB(apDescriptor, "F530");
               break;
            case 'p': /* dark pink */
               pCopyFrom = ColourRGB(apDescriptor, "F301");
               break;
            case 'P': /* light pink */
               pCopyFrom = ColourRGB(apDescriptor, "F502");
               break;
            case 't': /* dark tan */
               pCopyFrom = ColourRGB(apDescriptor, "F210");
               break;
            case 'T': /* light tan */
               pCopyFrom = ColourRGB(apDescriptor, "F321");
               break;
            case 'v': /* dark violet */
               pCopyFrom = ColourRGB(apDescriptor, "F104");
               break;
            case 'V': /* light violet */
               pCopyFrom = ColourRGB(apDescriptor, "F205");
               break;
            case '(': /* MXP link */
               if ( !pProtocol->bBlockMXP && pProtocol->pVariables[eMSDP_MXP]->ValueInt )
                  pCopyFrom = LinkStart;
               break;
            case ')': /* MXP link */
               if ( !pProtocol->bBlockMXP && pProtocol->pVariables[eMSDP_MXP]->ValueInt )
                  pCopyFrom = LinkStop;
               pProtocol->bBlockMXP = false;
               break;
            case '<':
               if ( !pProtocol->bBlockMXP && pProtocol->pVariables[eMSDP_MXP]->ValueInt )
               {
                  pCopyFrom = MXPStart;
                  bUseMXP = true;
               }
               else /* No MXP support, so just strip it out */
               {
                  while ( apData[j] != '\0' && apData[j] != '>' )
                     ++j;
               }
               pProtocol->bBlockMXP = false;
               break;
            case '[':
               if ( tolower(apData[++j]) == 'u' )
               {
                  char Buffer[8] = {'\0'}, BugString[256];
                  int Index = 0;
                  int Number = 0;
                  bool_t bDone = false, bValid = true;

                  while ( isdigit(apData[++j]) )
                  {
                     Number *= 10;
                     Number += (apData[j])-'0';
                  }

                  if ( apData[j] == '/' )
                     ++j;

                  while ( apData[j] != '\0' && !bDone )
                  {
                     if ( apData[j] == ']' )
                        bDone = true;
                     else if ( Index < 7 )
                        Buffer[Index++] = apData[j++];
                     else /* It's too long, so ignore the rest and note the problem */
                     {
                        j++;
                        bValid = false;
                     }
                  }

                  if ( !bDone )
                  {
                     sprintf( BugString, "BUG: Unicode substitute '%s' wasn't terminated with ']'.\n", Buffer );
                     ReportBug( BugString );
                  }
                  else if ( !bValid )
                  {
                     sprintf( BugString, "BUG: Unicode substitute '%s' truncated.  Missing ']'?\n", Buffer );
                     ReportBug( BugString );
                  }
                  else if ( pProtocol->pVariables[eMSDP_UTF_8]->ValueInt )
                  {
                     pCopyFrom = UnicodeGet(Number);
                  }
                  else /* Display the substitute string */
                  {
                     pCopyFrom = Buffer;
                  }

                  /* Terminate if we've reached the end of the string */
                  bTerminate = !bDone;
               }
               else if ( tolower(apData[j]) == 'f' || tolower(apData[j]) == 'b' )
               {
                  char Buffer[8] = {'\0'}, BugString[256];
                  int Index = 0;
                  bool_t bDone = false, bValid = true;

                  /* Copy the 'f' (foreground) or 'b' (background) */
                  Buffer[Index++] = apData[j++];

                  while ( apData[j] != '\0' && !bDone && bValid )
                  {
                     if ( apData[j] == ']' )
                        bDone = true;
                     else if ( Index < 4 )
                        Buffer[Index++] = apData[j++];
                     else /* It's too long, so drop out - the colour code may still be valid */
                        bValid = false;
                  }

                  if ( !bDone || !bValid )
                  {
                     sprintf( BugString, "BUG: RGB %sground colour '%s' wasn't terminated with ']'.\n", 
                        (tolower(Buffer[0]) == 'f') ? "fore" : "back", &Buffer[1] );
                     ReportBug( BugString );
                  }
                  else if ( !IsValidColour(Buffer) )
                  {
                     sprintf( BugString, "BUG: RGB %sground colour '%s' invalid (each digit must be in the range 0-5).\n", 
                        (tolower(Buffer[0]) == 'f') ? "fore" : "back", &Buffer[1] );
                     ReportBug( BugString );
                  }
                  else /* Success */
                  {
                     pCopyFrom = ColourRGB(apDescriptor, Buffer);
                  }
               }
               else if ( tolower(apData[j]) == 'x' )
               {
                  char Buffer[8] = {'\0'}, BugString[256];
                  int Index = 0;
                  bool_t bDone = false, bValid = true;

                  ++j; /* Skip the 'x' */

                  while ( apData[j] != '\0' && !bDone )
                  {
                     if ( apData[j] == ']' )
                        bDone = true;
                     else if ( Index < 7 )
                        Buffer[Index++] = apData[j++];
                     else /* It's too long, so ignore the rest and note the problem */
                     {
                        j++;
                        bValid = false;
                     }
                  }

                  if ( !bDone )
                  {
                     sprintf( BugString, "BUG: Required MXP version '%s' wasn't terminated with ']'.\n", Buffer );
                     ReportBug( BugString );
                  }
                  else if ( !bValid )
                  {
                     sprintf( BugString, "BUG: Required MXP version '%s' too long.  Missing ']'?\n", Buffer );
                     ReportBug( BugString );
                  }
                  else if ( !strcmp(pProtocol->pMXPVersion, "Unknown") || 
                     strcmp(pProtocol->pMXPVersion, Buffer) < 0 )
                  {
                     /* Their version of MXP isn't high enough */
                     pProtocol->bBlockMXP = true;
                  }
                  else /* MXP is sufficient for this tag */
                  {
                     pProtocol->bBlockMXP = false;
                  }

                  /* Terminate if we've reached the end of the string */
                  bTerminate = !bDone;
               }
               break;
            case '!': /* Used for in-band MSP sound triggers */
               pCopyFrom = MSP;
               break;
#ifdef COLOUR_CHAR
            case '+':
               bColourOn = true;
               break;
            case '-':
               bColourOn = false;
               break;
#endif /* COLOUR_CHAR */
            case '\0':
               bTerminate = true;
               break;
            default:
               break;
         }

         /* Copy the colour code, if any. */
         if ( pCopyFrom != NULL )
         {
            while ( *pCopyFrom != '\0' && i < MAX_OUTPUT_BUFFER )
               Result[i++] = *pCopyFrom++;
         }
      }
#ifdef COLOUR_CHAR
      else if ( bColourOn && apData[j] == COLOUR_CHAR )
      {
         const char ColourChar[] = { COLOUR_CHAR, '\0' };
         const char *pCopyFrom = NULL;

         switch ( apData[++j] )
         {
            case COLOUR_CHAR: /* Two in a row display the actual character */
               pCopyFrom = ColourChar;
               break;
            case 'n':
               pCopyFrom = s_Clean;
               break;
            case 'r': /* dark red */
               pCopyFrom = ColourRGB(apDescriptor, "F200");
               break;
            case 'R': /* light red */
               pCopyFrom = ColourRGB(apDescriptor, "F500");
               break;
            case 'g': /* dark green */
               pCopyFrom = ColourRGB(apDescriptor, "F020");
               break;
            case 'G': /* light green */
               pCopyFrom = ColourRGB(apDescriptor, "F050");
               break;
            case 'y': /* dark yellow */
               pCopyFrom = ColourRGB(apDescriptor, "F220");
               break;
            case 'Y': /* light yellow */
               pCopyFrom = ColourRGB(apDescriptor, "F550");
               break;
            case 'b': /* dark blue */
               pCopyFrom = ColourRGB(apDescriptor, "F002");
               break;
            case 'B': /* light blue */
               pCopyFrom = ColourRGB(apDescriptor, "F005");
               break;
            case 'm': /* dark magenta */
               pCopyFrom = ColourRGB(apDescriptor, "F202");
               break;
            case 'M': /* light magenta */
               pCopyFrom = ColourRGB(apDescriptor, "F505");
               break;
            case 'c': /* dark cyan */
               pCopyFrom = ColourRGB(apDescriptor, "F022");
               break;
            case 'C': /* light cyan */
               pCopyFrom = ColourRGB(apDescriptor, "F055");
               break;
            case 'w': /* dark white */
               pCopyFrom = ColourRGB(apDescriptor, "F222");
               break;
            case 'W': /* light white */
               pCopyFrom = ColourRGB(apDescriptor, "F555");
               break;
            case 'a': /* dark azure */
               pCopyFrom = ColourRGB(apDescriptor, "F014");
               break;
            case 'A': /* light azure */
               pCopyFrom = ColourRGB(apDescriptor, "F025");
               break;
            case 'j': /* dark jade */
               pCopyFrom = ColourRGB(apDescriptor, "F031");
               break;
            case 'J': /* light jade */
               pCopyFrom = ColourRGB(apDescriptor, "F052");
               break;
            case 'l': /* dark lime */
               pCopyFrom = ColourRGB(apDescriptor, "F140");
               break;
            case 'L': /* light lime */
               pCopyFrom = ColourRGB(apDescriptor, "F250");
               break;
            case 'o': /* dark orange */
               pCopyFrom = ColourRGB(apDescriptor, "F520");
               break;
            case 'O': /* light orange */
               pCopyFrom = ColourRGB(apDescriptor, "F530");
               break;
            case 'p': /* dark pink */
               pCopyFrom = ColourRGB(apDescriptor, "F301");
               break;
            case 'P': /* light pink */
               pCopyFrom = ColourRGB(apDescriptor, "F502");
               break;
            case 't': /* dark tan */
               pCopyFrom = ColourRGB(apDescriptor, "F210");
               break;
            case 'T': /* light tan */
               pCopyFrom = ColourRGB(apDescriptor, "F321");
               break;
            case 'v': /* dark violet */
               pCopyFrom = ColourRGB(apDescriptor, "F104");
               break;
            case 'V': /* light violet */
               pCopyFrom = ColourRGB(apDescriptor, "F205");
               break;
            case '\0':
               bTerminate = true;
               break;
#ifdef EXTENDED_COLOUR
            case '[':
               if ( tolower(apData[++j]) == 'f' || tolower(apData[j]) == 'b' )
               {
                  char Buffer[8] = {'\0'};
                  int Index = 0;
                  bool_t bDone = false, bValid = true;

                  /* Copy the 'f' (foreground) or 'b' (background) */
                  Buffer[Index++] = apData[j++];

                  while ( apData[j] != '\0' && !bDone && bValid )
                  {
                     if ( apData[j] == ']' )
                        bDone = true;
                     else if ( Index < 4 )
                        Buffer[Index++] = apData[j++];
                     else /* It's too long, so drop out - the colour code may still be valid */
                        bValid = false;
                  }

                  if ( bDone && bValid && IsValidColour(Buffer) )
                     pCopyFrom = ColourRGB(apDescriptor, Buffer);
               }
               break;
#endif /* EXTENDED_COLOUR */
            default:
#ifdef DISPLAY_INVALID_COLOUR_CODES
               Result[i++] = COLOUR_CHAR;
               Result[i++] = apData[j];
#endif /* DISPLAY_INVALID_COLOUR_CODES */
               break;
         }

         /* Copy the colour code, if any. */
         if ( pCopyFrom != NULL )
         {
            while ( *pCopyFrom != '\0' && i < MAX_OUTPUT_BUFFER )
               Result[i++] = *pCopyFrom++;
         }
      }
#endif /* COLOUR_CHAR */
      else if ( bUseMXP && apData[j] == '>' )
      {
         const char *pCopyFrom = MXPStop;
         while ( *pCopyFrom != '\0' && i < MAX_OUTPUT_BUFFER)
            Result[i++] = *pCopyFrom++;
         bUseMXP = false;
      }
      else if ( bUseMSP && j > 0 && apData[j-1] == '!' && apData[j] == '!' && 
         PrefixString("SOUND(", &apData[j+1]) )
      {
         /* Avoid accidental triggering of old-style MSP triggers */
         Result[i++] = '?';
      }
      else /* Just copy the character normally */
      {
         Result[i++] = apData[j];
      }
   }

   /* If we'd overflow the buffer, we don't send any output */
   if ( i >= MAX_OUTPUT_BUFFER )
   {
      i = 0;
      ReportBug("ProtocolOutput: Too much outgoing data to store in the buffer.\n");
   }

   /* Terminate the string */
   Result[i] = '\0';

   /* Store the length */
   if ( apLength )
      *apLength = i;

   /* Return the string */
   return Result;
}

/* Some clients (such as GMud) don't properly handle negotiation, and simply 
 * display every printable character to the screen.  However TTYPE isn't a 
 * printable character, so we negotiate for it first, and only negotiate for 
 * other protocols if the client responds with IAC WILL TTYPE or IAC WONT 
 * TTYPE.  Thanks go to Donky on MudBytes for the suggestion.
 */
void ProtocolNegotiate( descriptor_t *apDescriptor )
{
   ConfirmNegotiation(apDescriptor, eNEGOTIATED_TTYPE, true, true);
}

/* Tells the client to switch echo on or off. */
void ProtocolNoEcho( descriptor_t *apDescriptor, bool_t abOn )
{
   ConfirmNegotiation(apDescriptor, eNEGOTIATED_ECHO, abOn, true);
}

/******************************************************************************
 Copyover save/load functions.
 ******************************************************************************/

const char *CopyoverGet( descriptor_t *apDescriptor )
{
   static char Buffer[64];
   char *pBuffer = Buffer;
   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

   if ( pProtocol != NULL )
   {
      sprintf(Buffer, "%d/%d", pProtocol->ScreenWidth, pProtocol->ScreenHeight);

      /* Skip to the end */
      while ( *pBuffer != '\0' )
         ++pBuffer;

      if ( pProtocol->bTTYPE )
         *pBuffer++ = 'T';
      if ( pProtocol->bNAWS )
         *pBuffer++ = 'N';
      if ( pProtocol->bMSDP )
         *pBuffer++ = 'M';
      if ( pProtocol->bATCP )
         *pBuffer++ = 'A';
      if ( pProtocol->bMSP )
         *pBuffer++ = 'S';
      if ( pProtocol->pVariables[eMSDP_MXP]->ValueInt )
         *pBuffer++ = 'X';
      if ( pProtocol->bMCCP )
      {
         *pBuffer++ = 'c';
         CompressEnd(apDescriptor);
      }
      if ( pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt )
         *pBuffer++ = 'C';
      if ( pProtocol->bCHARSET )
         *pBuffer++ = 'H';
      if ( pProtocol->pVariables[eMSDP_UTF_8]->ValueInt )
         *pBuffer++ = 'U';

		/*************** START GMCP ***************/
		if ( pProtocol->bGMCP )
			*pBuffer++ = 'G';
		if ( pProtocol->bSGA )
			*pBuffer++ = 'g';
		/*************** END GMCP ***************/
   }

   /* Terminate the string */
   *pBuffer = '\0';

   return Buffer;
}

void CopyoverSet( descriptor_t *apDescriptor, const char *apData )
{
   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

   if ( pProtocol != NULL && apData != NULL )
   {
      int Width = 0, Height = 0;
      bool_t bDoneWidth = false;
      int i; /* Loop counter */

      for ( i = 0; apData[i] != '\0'; ++i )
      {
         switch ( apData[i] )
         {
            case 'T':
               pProtocol->bTTYPE = true;
               break;
            case 'N':
               pProtocol->bNAWS = true;
               break;
            case 'M':
               pProtocol->bMSDP = true;
               break;
            case 'A':
               pProtocol->bATCP = true;
               break;
            case 'S':
               pProtocol->bMSP = true;
               break;
            case 'X':
               pProtocol->bMXP = true;
               pProtocol->pVariables[eMSDP_MXP]->ValueInt = 1;
               break;
            case 'c':
               pProtocol->bMCCP = true;
               CompressStart(apDescriptor);
               break;
            case 'C':
               pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
               break;
            case 'H':
               pProtocol->bCHARSET = true;
               break;
            case 'U':
               pProtocol->pVariables[eMSDP_UTF_8]->ValueInt = 1;
               break;
            default:
               if ( apData[i] == '/' )
                  bDoneWidth = true;
               else if ( isdigit(apData[i]) )
               {
                  if ( bDoneWidth )
                  {
                     Height *= 10;
                     Height += (apData[i] - '0');
                  }
                  else /* We're still calculating height */
                  {
                     Width *= 10;
                     Width += (apData[i] - '0');
                  }
               }
               break;

			/*************** START GMCP ***************/
			case 'G':
				pProtocol->bGMCP = true;
			break;
			case 'g':
				pProtocol->bSGA = true;
			break;
			/*************** END GMCP ***************/
         }
      }

      /* Restore the width and height */
      pProtocol->ScreenWidth = Width;
      pProtocol->ScreenHeight = Height;

      /* If we're using MSDP or ATCP, we need to renegotiate it so that the 
       * client can resend the list of variables it wants us to REPORT.
       *
       * Note that we only use ATCP if MSDP is not supported.
       */
      if ( pProtocol->bMSDP )
      {
         ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, true, true);
      }
      else if ( pProtocol->bATCP )
      {
         ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, true, true);
      }

      /* Ask the client to send its MXP version again */
      if ( pProtocol->bMXP )
         MXPSendTag( apDescriptor, "<VERSION>" );
   }
}

/******************************************************************************
 MSDP global functions.
 ******************************************************************************/

void MSDPUpdate( descriptor_t *apDescriptor )
{
   int i; /* Loop counter */

   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

   for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
   {
      if ( pProtocol->pVariables[i]->bReport )
      {
         if ( pProtocol->pVariables[i]->bDirty )
         {
            MSDPSend( apDescriptor, (variable_t)i );
            pProtocol->pVariables[i]->bDirty = false;
         }
      }
   }
}

void MSDPFlush( descriptor_t *apDescriptor, variable_t aMSDP )
{
   if ( aMSDP > eMSDP_NONE && aMSDP < eMSDP_MAX )
   {
      protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

      if ( pProtocol->pVariables[aMSDP]->bReport )
      {
         if ( pProtocol->pVariables[aMSDP]->bDirty )
         {
            MSDPSend( apDescriptor, aMSDP );
            pProtocol->pVariables[aMSDP]->bDirty = false;
         }
      }
   }
}

void MSDPSend( descriptor_t *apDescriptor, variable_t aMSDP )
{
   char MSDPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

   if ( aMSDP > eMSDP_NONE && aMSDP < eMSDP_MAX )
   {
      protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

      if ( VariableNameTable[aMSDP].bString )
      {
         /* Should really be replaced with a dynamic buffer */
         int RequiredBuffer = strlen(VariableNameTable[aMSDP].pName) + 
            strlen(pProtocol->pVariables[aMSDP]->pValueString) + 12;

         if ( RequiredBuffer >= MAX_VARIABLE_LENGTH )
         {
            sprintf( MSDPBuffer, 
               "MSDPSend: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", 
               VariableNameTable[aMSDP].pName, RequiredBuffer, 
               MAX_VARIABLE_LENGTH );
            ReportBug( MSDPBuffer );
            MSDPBuffer[0] = '\0';
         }
         else if ( pProtocol->bMSDP )
         {
            sprintf( MSDPBuffer, "%c%c%c%c%s%c%s%c%c", 
               IAC, SB, TELOPT_MSDP, MSDP_VAR, 
               VariableNameTable[aMSDP].pName, MSDP_VAL, 
               pProtocol->pVariables[aMSDP]->pValueString, IAC, SE );
         }
         else if ( pProtocol->bATCP )
         {
            sprintf( MSDPBuffer, "%c%c%cMSDP.%s %s%c%c", 
               IAC, SB, TELOPT_ATCP, 
               VariableNameTable[aMSDP].pName, 
               pProtocol->pVariables[aMSDP]->pValueString, IAC, SE );
         }
      }
      else /* It's an integer, not a string */
      {
         if ( pProtocol->bMSDP )
         {
            sprintf( MSDPBuffer, "%c%c%c%c%s%c%d%c%c", 
               IAC, SB, TELOPT_MSDP, MSDP_VAR, 
               VariableNameTable[aMSDP].pName, MSDP_VAL, 
               pProtocol->pVariables[aMSDP]->ValueInt, IAC, SE );
         }
         else if ( pProtocol->bATCP )
         {
            sprintf( MSDPBuffer, "%c%c%cMSDP.%s %d%c%c", 
               IAC, SB, TELOPT_ATCP, 
               VariableNameTable[aMSDP].pName, 
               pProtocol->pVariables[aMSDP]->ValueInt, IAC, SE );
         }
      }

      /* Just in case someone calls this function without checking MSDP/ATCP */
      if ( MSDPBuffer[0] != '\0' )
         Write( apDescriptor, MSDPBuffer );
   }
}

void MSDPSendPair( descriptor_t *apDescriptor, const char *apVariable, const char *apValue )
{
   char MSDPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

   if ( apVariable != NULL && apValue != NULL )
   {
      protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

      /* Should really be replaced with a dynamic buffer */
      int RequiredBuffer = strlen(apVariable) + strlen(apValue) + 12;

      if ( RequiredBuffer >= MAX_VARIABLE_LENGTH )
      {
         if ( RequiredBuffer - strlen(apValue) < MAX_VARIABLE_LENGTH )
         {
            sprintf( MSDPBuffer, 
               "MSDPSendPair: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", 
               apVariable, RequiredBuffer, MAX_VARIABLE_LENGTH );
         }
         else /* The variable name itself is too long */
         {
            sprintf( MSDPBuffer, 
               "MSDPSendPair: Variable name has a length of %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", 
               RequiredBuffer, MAX_VARIABLE_LENGTH );
         }

         ReportBug( MSDPBuffer );
         MSDPBuffer[0] = '\0';
      }
      else if ( pProtocol->bMSDP )
      {
         sprintf( MSDPBuffer, "%c%c%c%c%s%c%s%c%c", 
            IAC, SB, TELOPT_MSDP, MSDP_VAR, apVariable, MSDP_VAL, 
            apValue, IAC, SE );
      }
      else if ( pProtocol->bATCP )
      {
         sprintf( MSDPBuffer, "%c%c%cMSDP.%s %s%c%c", 
            IAC, SB, TELOPT_ATCP, apVariable, apValue, IAC, SE );
      }

      /* Just in case someone calls this function without checking MSDP/ATCP */
      if ( MSDPBuffer[0] != '\0' )
         Write( apDescriptor, MSDPBuffer );
   }
}

void MSDPSendList( descriptor_t *apDescriptor, const char *apVariable, const char *apValue )
{
   char MSDPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

   if ( apVariable != NULL && apValue != NULL )
   {
      protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

      /* Should really be replaced with a dynamic buffer */
      int RequiredBuffer = strlen(apVariable) + strlen(apValue) + 12;

      if ( RequiredBuffer >= MAX_VARIABLE_LENGTH )
      {
         if ( RequiredBuffer - strlen(apValue) < MAX_VARIABLE_LENGTH )
         {
            sprintf( MSDPBuffer, 
               "MSDPSendList: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", 
               apVariable, RequiredBuffer, MAX_VARIABLE_LENGTH );
         }
         else /* The variable name itself is too long */
         {
            sprintf( MSDPBuffer, 
               "MSDPSendList: Variable name has a length of %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", 
               RequiredBuffer, MAX_VARIABLE_LENGTH );
         }

         ReportBug( MSDPBuffer );
         MSDPBuffer[0] = '\0';
      }
      else if ( pProtocol->bMSDP )
      {
         int i; /* Loop counter */
         sprintf( MSDPBuffer, "%c%c%c%c%s%c%c%c%s%c%c%c", 
            IAC, SB, TELOPT_MSDP, MSDP_VAR, apVariable, MSDP_VAL, 
            MSDP_ARRAY_OPEN, MSDP_VAL, apValue, MSDP_ARRAY_CLOSE, IAC, SE );

         /* Convert the spaces to MSDP_VAL */
         for ( i = 0; MSDPBuffer[i] != '\0'; ++i )
         {
            if ( MSDPBuffer[i] == ' ' )
               MSDPBuffer[i] = MSDP_VAL;
         }
      }
      else if ( pProtocol->bATCP )
      {
         sprintf( MSDPBuffer, "%c%c%cMSDP.%s %s%c%c", 
            IAC, SB, TELOPT_ATCP, apVariable, apValue, IAC, SE );
      }

      /* Just in case someone calls this function without checking MSDP/ATCP */
      if ( MSDPBuffer[0] != '\0' )
         Write( apDescriptor, MSDPBuffer );
   }
}

void MSDPSetNumber( descriptor_t *apDescriptor, variable_t aMSDP, int aValue )
{
   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

   if ( pProtocol != NULL && aMSDP > eMSDP_NONE && aMSDP < eMSDP_MAX )
   {
      if ( !VariableNameTable[aMSDP].bString )
      {
         if ( pProtocol->pVariables[aMSDP]->ValueInt != aValue )
         {
            pProtocol->pVariables[aMSDP]->ValueInt = aValue;
            pProtocol->pVariables[aMSDP]->bDirty = true;
         }
      }
   }
}

void MSDPSetString( descriptor_t *apDescriptor, variable_t aMSDP, const char *apValue )
{
   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

   if ( pProtocol != NULL && apValue != NULL )
   {
      if ( VariableNameTable[aMSDP].bString )
      {
         if ( strcmp(pProtocol->pVariables[aMSDP]->pValueString, apValue) )
         {
            free(pProtocol->pVariables[aMSDP]->pValueString);
            pProtocol->pVariables[aMSDP]->pValueString = AllocString(apValue);
            pProtocol->pVariables[aMSDP]->bDirty = true;
         }
      }
   }
}

void MSDPSetTable( descriptor_t *apDescriptor, variable_t aMSDP, const char *apValue )
{
   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

   if ( pProtocol != NULL && apValue != NULL )
   {
      if ( *apValue == '\0' )
      {
         /* It's easier to call MSDPSetString if the value is empty */
         MSDPSetString(apDescriptor, aMSDP, apValue);
      }
      else if ( VariableNameTable[aMSDP].bString )
      {
         const char MsdpTableStart[] = { (char)MSDP_TABLE_OPEN, '\0' };
         const char MsdpTableStop[]  = { (char)MSDP_TABLE_CLOSE, '\0' };

         char *pTable = malloc(strlen(apValue) + 3); /* 3: START, STOP, NUL */

         strcpy(pTable, MsdpTableStart);
         strcat(pTable, apValue);
         strcat(pTable, MsdpTableStop);

         if ( strcmp(pProtocol->pVariables[aMSDP]->pValueString, pTable) )
         {
            free(pProtocol->pVariables[aMSDP]->pValueString);
            pProtocol->pVariables[aMSDP]->pValueString = pTable;
            pProtocol->pVariables[aMSDP]->bDirty = true;
         }
         else /* Just discard the table, we've already got one */
         {
            free(pTable);
         }
      }
   }
}

void MSDPSetArray( descriptor_t *apDescriptor, variable_t aMSDP, const char *apValue )
{
   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

   if ( pProtocol != NULL && apValue != NULL )
   {
      if ( *apValue == '\0' )
      {
         /* It's easier to call MSDPSetString if the value is empty */
         MSDPSetString(apDescriptor, aMSDP, apValue);
      }
      else if ( VariableNameTable[aMSDP].bString )
      {
         const char MsdpArrayStart[] = { (char)MSDP_ARRAY_OPEN, '\0' };
         const char MsdpArrayStop[]  = { (char)MSDP_ARRAY_CLOSE, '\0' };

         char *pArray = malloc(strlen(apValue) + 3); /* 3: START, STOP, NUL */

         strcpy(pArray, MsdpArrayStart);
         strcat(pArray, apValue);
         strcat(pArray, MsdpArrayStop);

         if ( strcmp(pProtocol->pVariables[aMSDP]->pValueString, pArray) )
         {
            free(pProtocol->pVariables[aMSDP]->pValueString);
            pProtocol->pVariables[aMSDP]->pValueString = pArray;
            pProtocol->pVariables[aMSDP]->bDirty = true;
         }
         else /* Just discard the array, we've already got one */
         {
            free(pArray);
         }
      }
   }
}

/******************************************************************************
 MSSP global functions.
 ******************************************************************************/

void MSSPSetPlayers( int aPlayers )
{
   s_Players = aPlayers;

   if ( s_Uptime == 0 )
      s_Uptime = time(0);
}

/******************************************************************************
 MXP global functions.
 ******************************************************************************/

const char *MXPCreateTag( descriptor_t *apDescriptor, const char *apTag )
{
   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

   if ( pProtocol != NULL && pProtocol->pVariables[eMSDP_MXP]->ValueInt && 
      strlen(apTag) < 1000 )
   {
      static char MXPBuffer [1024];
      sprintf( MXPBuffer, "\033[1z%s\033[7z", apTag );
      return MXPBuffer;
   }
   else /* Leave the tag as-is, don't try to MXPify it */
   {
      return apTag;
   }
}

void MXPSendTag( descriptor_t *apDescriptor, const char *apTag )
{
   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

   if ( pProtocol != NULL && apTag != NULL && strlen(apTag) < 1000 )
   {
      if ( pProtocol->pVariables[eMSDP_MXP]->ValueInt )
      {
         char MXPBuffer [1024];
         sprintf(MXPBuffer, "\033[1z%s\033[7z\r\n", apTag );
         Write(apDescriptor, MXPBuffer);
      }
      else if ( pProtocol->bRenegotiate )
      {
         /* Tijer pointed out that when MUSHclient autoconnects, it fails 
          * to complete the negotiation.  This workaround will attempt to 
          * renegotiate after the character has connected.
          */

         int i; /* Renegotiate everything except TTYPE */
         for ( i = eNEGOTIATED_TTYPE+1; i < eNEGOTIATED_MAX; ++i )
         {
            pProtocol->Negotiated[i] = false;
            ConfirmNegotiation(apDescriptor, (negotiated_t)i, true, true);
         }

         pProtocol->bRenegotiate = false;
         pProtocol->bNeedMXPVersion = true;
         Negotiate(apDescriptor);
      }
   }
}

/******************************************************************************
 Sound global functions.
 ******************************************************************************/

void SoundSend( descriptor_t *apDescriptor, const char *apTrigger )
{
   const int MaxTriggerLength = 128; /* Used for the buffer size */

   if ( apDescriptor != NULL && apTrigger != NULL )
   {
      protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

      if ( pProtocol != NULL && pProtocol->pVariables[eMSDP_SOUND]->ValueInt )
      {
         if ( pProtocol->bMSDP || pProtocol->bATCP )
         {
            /* Send the sound trigger through MSDP or ATCP */
            MSDPSendPair( apDescriptor, "PLAY_SOUND", apTrigger );
         }
         else if ( strlen(apTrigger) <= MaxTriggerLength )
         {
            /* Use an old MSP-style trigger */
            char *pBuffer = alloca(MaxTriggerLength+10);
            sprintf( pBuffer, "\t!SOUND(%s)", apTrigger );
            Write(apDescriptor, pBuffer);
         }
      }
   }
}

/******************************************************************************
 Colour global functions.
 ******************************************************************************/

const char *ColourRGB( descriptor_t *apDescriptor, const char *apRGB )
{
   protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

   if ( pProtocol && pProtocol->pVariables[eMSDP_ANSI_COLORS]->ValueInt )
   {
      if ( IsValidColour(apRGB) )
      {
         bool_t bBackground = (tolower(apRGB[0]) == 'b');
         int Red = apRGB[1] - '0';
         int Green = apRGB[2] - '0';
         int Blue = apRGB[3] - '0';

         if ( pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt )
            return GetRGBColour( bBackground, Red, Green, Blue );
         else /* Use regular ANSI colour */
            return GetAnsiColour( bBackground, Red, Green, Blue );
      }
      else /* Invalid colour - use this to clear any existing colour. */
      {
         return s_Clean;
      }
   }
   else /* Don't send any colour, not even clear */
   {
      return "";
   }
}

/******************************************************************************
 UTF-8 global functions.
 ******************************************************************************/

char *UnicodeGet( int aValue )
{
   static char Buffer[8];
   char *pString = Buffer;

   UnicodeAdd( &pString, aValue );
   *pString = '\0';

   return Buffer;
}

void UnicodeAdd( char **apString, int aValue )
{
   if ( aValue < 0x80 )
   {
      *(*apString)++ = (char)aValue;
   }
   else if ( aValue < 0x800 )
   {
      *(*apString)++ = (char)(0xC0 | (aValue>>6));
      *(*apString)++ = (char)(0x80 | (aValue & 0x3F));
   }
   else if ( aValue < 0x10000 )
   {
      *(*apString)++ = (char)(0xE0 | (aValue>>12));
      *(*apString)++ = (char)(0x80 | (aValue>>6 & 0x3F));
      *(*apString)++ = (char)(0x80 | (aValue & 0x3F));
   }
   else if ( aValue < 0x200000 )
   {
      *(*apString)++ = (char)(0xF0 | (aValue>>18));
      *(*apString)++ = (char)(0x80 | (aValue>>12 & 0x3F));
      *(*apString)++ = (char)(0x80 | (aValue>>6 & 0x3F));
      *(*apString)++ = (char)(0x80 | (aValue & 0x3F));
   }
}

/******************************************************************************
 Local negotiation functions.
 ******************************************************************************/

static void Negotiate( descriptor_t *apDescriptor )
{
   protocol_t *pProtocol = apDescriptor->pProtocol;

   if ( pProtocol->bNegotiated )
   {
      const char RequestTTYPE   [] = { (char)IAC, (char)SB,   TELOPT_TTYPE, SEND, (char)IAC, (char)SE, '\0' };

      /* Request the client type if TTYPE is supported. */
      if ( pProtocol->bTTYPE )
         Write(apDescriptor, RequestTTYPE);

      /* Check for other protocols. */
      ConfirmNegotiation(apDescriptor, eNEGOTIATED_NAWS, true, true);
      ConfirmNegotiation(apDescriptor, eNEGOTIATED_CHARSET, true, true);
      ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, true, true);
      ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSSP, true, true);
      ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, true, true);
      ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSP, true, true);
      ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP, true, true);
      ConfirmNegotiation(apDescriptor, eNEGOTIATED_MCCP, true, true);

		/*************** START GMCP ***************/
		ConfirmNegotiation( apDescriptor, eNEGOTIATED_GMCP, true, true );
		ConfirmNegotiation( apDescriptor, eNEGOTIATED_SGA, true, true );
		/*************** END GMCP ***************/
   }
}

static void PerformHandshake( descriptor_t *apDescriptor, char aCmd, char aProtocol )
{
   protocol_t *pProtocol = apDescriptor->pProtocol;

   switch ( aProtocol )
   {
      case (char)TELOPT_TTYPE:
         if ( aCmd == (char)WILL )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_TTYPE, true, true);
            pProtocol->bTTYPE = true;

            if ( !pProtocol->bNegotiated )
            {
               /* Negotiate for the remaining protocols. */
               pProtocol->bNegotiated = true;
               Negotiate(apDescriptor);

               /* We may need to renegotiate if they don't reply */            
               pProtocol->bRenegotiate = true;
            }
         }
         else if ( aCmd == (char)WONT )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_TTYPE, false, pProtocol->bTTYPE);
            pProtocol->bTTYPE = false;

            if ( !pProtocol->bNegotiated )
            {
               /* Still negotiate, as this client obviously knows how to 
                * correctly respond to negotiation attempts - but we don't 
                * ask for TTYPE, as it doesn't support it.
                */
               pProtocol->bNegotiated = true;
               Negotiate(apDescriptor);

               /* We may need to renegotiate if they don't reply */            
               pProtocol->bRenegotiate = true;
            }
         }
         else if ( aCmd == (char)DO )
         {
            /* Invalid negotiation, send a rejection */
            SendNegotiationSequence( apDescriptor, (char)WONT, (char)aProtocol );
         }
         break;

      case (char)TELOPT_ECHO:
         if ( aCmd == (char)DO )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_ECHO, true, true);
            pProtocol->bECHO = true;
         }
         else if ( aCmd == (char)DONT )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_ECHO, false, pProtocol->bECHO);
            pProtocol->bECHO = false;
         }
         else if ( aCmd == (char)WILL )
         {
            /* Invalid negotiation, send a rejection */
            SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
         }
         break;

      case (char)TELOPT_NAWS:
         if ( aCmd == (char)WILL )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_NAWS, true, true);
            pProtocol->bNAWS = true;

            /* Renegotiation workaround won't be necessary. */
            pProtocol->bRenegotiate = false;
         }
         else if ( aCmd == (char)WONT )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_NAWS, false, pProtocol->bNAWS);
            pProtocol->bNAWS = false;

            /* Renegotiation workaround won't be necessary. */
            pProtocol->bRenegotiate = false;
         }
         else if ( aCmd == (char)DO )
         {
            /* Invalid negotiation, send a rejection */
            SendNegotiationSequence( apDescriptor, (char)WONT, (char)aProtocol );
         }
         break;

      case (char)TELOPT_CHARSET:
         if ( aCmd == (char)WILL )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_CHARSET, true, true);
            if ( !pProtocol->bCHARSET )
            {
               const char charset_utf8 [] = { (char)IAC, (char)SB, TELOPT_CHARSET, 1, ' ', 'U', 'T', 'F', '-', '8', (char)IAC, (char)SE, '\0' };
               Write(apDescriptor, charset_utf8);
               pProtocol->bCHARSET = true;
            }
         }
         else if ( aCmd == (char)WONT )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_CHARSET, false, pProtocol->bCHARSET);
            pProtocol->bCHARSET = false;
         }
         else if ( aCmd == (char)DO )
         {
            /* Invalid negotiation, send a rejection */
            SendNegotiationSequence( apDescriptor, (char)WONT, (char)aProtocol );
         }
         break;

      case (char)TELOPT_MSDP:
         if ( aCmd == (char)DO )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, true, true);

            if ( !pProtocol->bMSDP )
            {
               pProtocol->bMSDP = true;

               /* Identify the mud to the client. */
               MSDPSendPair( apDescriptor, "SERVER_ID", MUD_NAME );
            }
         }
         else if ( aCmd == (char)DONT )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSDP, false, pProtocol->bMSDP);
            pProtocol->bMSDP = false;
         }
         else if ( aCmd == (char)WILL )
         {
            /* Invalid negotiation, send a rejection */
            SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
         }
         break;

      case (char)TELOPT_MSSP:
         if ( aCmd == (char)DO )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSSP, true, true);

            if ( !pProtocol->bMSSP )
            {
               SendMSSP( apDescriptor );
               pProtocol->bMSSP = true;
            }
         }
         else if ( aCmd == (char)DONT )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSSP, false, pProtocol->bMSSP);
            pProtocol->bMSSP = false;
         }
         else if ( aCmd == (char)WILL )
         {
            /* Invalid negotiation, send a rejection */
            SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
         }
         break;

      case (char)TELOPT_MCCP:
         if ( aCmd == (char)DO )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_MCCP, true, true);

            if ( !pProtocol->bMCCP )
            {
               pProtocol->bMCCP = true;
               CompressStart( apDescriptor );
            }
         }
         else if ( aCmd == (char)DONT )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_MCCP, false, pProtocol->bMCCP);

            if ( pProtocol->bMCCP )
            {
               pProtocol->bMCCP = false;
               CompressEnd( apDescriptor );
            }
         }
         else if ( aCmd == (char)WILL )
         {
            /* Invalid negotiation, send a rejection */
            SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
         }
         break;

      case (char)TELOPT_MSP:
         if ( aCmd == (char)DO )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSP, true, true);
            pProtocol->bMSP = true;
         }
         else if ( aCmd == (char)DONT )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_MSP, false, pProtocol->bMSP);
            pProtocol->bMSP = false;
         }
         else if ( aCmd == (char)WILL )
         {
            /* Invalid negotiation, send a rejection */
            SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
         }
         break;

      case (char)TELOPT_MXP:
         if ( aCmd == (char)WILL || aCmd == (char)DO )
         {
            if ( aCmd == (char)WILL )
               ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP, true, true);
            else /* aCmd == (char)DO */
               ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP2, true, true);

            if ( !pProtocol->bMXP )
            {
               /* Enable MXP. */
               const char EnableMXP[] = { (char)IAC, (char)SB, TELOPT_MXP, (char)IAC, (char)SE, '\0' };
               Write(apDescriptor, EnableMXP);

               /* Create a secure channel, and note that MXP is active. */
               Write(apDescriptor, "\033[7z");
               pProtocol->bMXP = true;
               pProtocol->pVariables[eMSDP_MXP]->ValueInt = 1;

               if ( pProtocol->bNeedMXPVersion )
                  MXPSendTag( apDescriptor, "<VERSION>" );
            }
         }
         else if ( aCmd == (char)WONT )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP, false, pProtocol->bMXP);

            if ( !pProtocol->bMXP )
            {
               /* The MXP standard doesn't actually specify whether you should 
                * negotiate with IAC DO MXP or IAC WILL MXP.  As a result, some 
                * clients support one, some the other, and some support both.
                * 
                * Therefore we first try IAC DO MXP, and if the client replies 
                * with WONT, we try again (here) with IAC WILL MXP.
                */
               ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP2, true, true);
            }
            else /* The client is actually asking us to switch MXP off. */
            {
               pProtocol->bMXP = false;
            }
         }
         else if ( aCmd == (char)DONT )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_MXP2, false, pProtocol->bMXP);
            pProtocol->bMXP = false;
         }
         break;

      case (char)TELOPT_ATCP:
         if ( aCmd == (char)WILL )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, true, true);

            /* If we don't support MSDP, fake it with ATCP */
            if ( !pProtocol->bMSDP && !pProtocol->bATCP )
            {
               pProtocol->bATCP = true;

#ifdef MUDLET_PACKAGE
               /* Send the Mudlet GUI package to the user. */
               if ( MatchString( "Mudlet",
                  pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString ) )
               {
                  SendATCP( apDescriptor, "Client.GUI", MUDLET_PACKAGE );
               }
#endif /* MUDLET_PACKAGE */

               /* Identify the mud to the client. */
               MSDPSendPair( apDescriptor, "SERVER_ID", MUD_NAME );
            }
         }
         else if ( aCmd == (char)WONT )
         {
            ConfirmNegotiation(apDescriptor, eNEGOTIATED_ATCP, false, pProtocol->bATCP);
            pProtocol->bATCP = false;
         }
         else if ( aCmd == (char)DO )
         {
            /* Invalid negotiation, send a rejection */
            SendNegotiationSequence( apDescriptor, (char)WONT, (char)aProtocol );
         }
         break;

      default:
         if ( aCmd == (char)WILL )
         {
            /* Invalid negotiation, send a rejection */
            SendNegotiationSequence( apDescriptor, (char)DONT, (char)aProtocol );
         }
         else if ( aCmd == (char)DO )
         {
            /* Invalid negotiation, send a rejection */
            SendNegotiationSequence( apDescriptor, (char)WONT, (char)aProtocol );
         }
         break;

	/*************** START GMCP ***************/
	case ( char ) TELOPT_GMCP:
		if ( aCmd == ( char ) DONT )
		{
			ConfirmNegotiation( apDescriptor, eNEGOTIATED_GMCP, false, pProtocol->bGMCP );
            pProtocol->bGMCP = false;
		}
		else if ( aCmd == ( char ) DO )
		{
			ConfirmNegotiation( apDescriptor, eNEGOTIATED_GMCP, true, true );
            pProtocol->bGMCP = true;
		}
		else if ( aCmd == ( char ) WILL )
		{
			/* Invalid negotiation, send a rejection */
			SendNegotiationSequence( apDescriptor, ( char ) WONT, ( char ) aProtocol );
		}
		else if ( aCmd == ( char ) WONT )
		{
			/* Invalid negotiation, send a rejection */
			SendNegotiationSequence( apDescriptor, ( char ) WONT, ( char ) aProtocol );
		}
	break;

	case ( char ) TELOPT_SGA:
		if ( aCmd == ( char ) DONT )
		{
			/* Invalid negotiation, send a rejection */
			SendNegotiationSequence( apDescriptor, ( char ) WONT, ( char ) aProtocol );
		}
		else if ( aCmd == ( char ) DO )
		{
			/* Invalid negotiation, send a rejection */
			SendNegotiationSequence( apDescriptor, ( char ) WONT, ( char ) aProtocol );
		}
		else if ( aCmd == ( char ) WILL )
		{
			ConfirmNegotiation( apDescriptor, eNEGOTIATED_SGA, true, false );
            pProtocol->bSGA = true;
		}
		else if ( aCmd == ( char ) WONT )
		{
			ConfirmNegotiation( apDescriptor, eNEGOTIATED_SGA, false, false );
            pProtocol->bSGA = false;
		}
	break;
	/*************** END GMCP ***************/

   }
}

static void PerformSubnegotiation( descriptor_t *apDescriptor, char aCmd, char *apData, int aSize )
{
   protocol_t *pProtocol = apDescriptor->pProtocol;

   switch ( aCmd )
   {
      case (char)TELOPT_TTYPE:
         if ( pProtocol->bTTYPE )
         {
            /* Store the client name. */
            const int MaxClientLength = 64;
            char *pClientName = alloca(MaxClientLength+1);
            int i = 0, j = 1;
            bool_t bStopCyclicTTYPE = false;

            for ( ; apData[j] != '\0' && i < MaxClientLength; ++j )
            {
               if ( isprint(apData[j]) )
                  pClientName[i++] = apData[j];
            }
            pClientName[i] = '\0';

            /* Store the first TTYPE as the client name */
            if ( !strcmp(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString, "Unknown") )
            {
               free(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString);
               pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pClientName);

               /* This is a bit nasty, but using cyclic TTYPE on windows telnet 
                * causes it to lock up.  None of the clients we need to cycle 
                * with send ANSI to start with anyway, so we shouldn't have any 
                * conflicts.
                * 
                * An alternative solution is to use escape sequences to check 
                * for windows telnet prior to negotiation, and this also avoids 
                * the player losing echo, but it has other issues.  Because the 
                * escape codes are technically in-band, even though they'll be 
                * stripped from the display, the newlines will still cause some 
                * scrolling.  Therefore you need to either pause the session 
                * for a few seconds before displaying the login screen, or wait 
                * until the player has entered their name before negotiating.
                */
               if ( !strcmp(pClientName,"ANSI") )
                  bStopCyclicTTYPE = true;
            }

            /* Cycle through the TTYPEs until we get the same result twice, or 
             * find ourselves back at the start.
             * 
             * If the client follows RFC1091 properly then it will indicate the 
             * end of the list by repeating the last response, and then return 
             * to the top of the list.  If you're the trusting type, then feel 
             * free to remove the second strcmp ;)
             */
            if ( pProtocol->pLastTTYPE == NULL || 
               (strcmp(pProtocol->pLastTTYPE, pClientName) && 
               strcmp(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString, pClientName)) )
            {
               char RequestTTYPE [] = { (char)IAC, (char)SB, TELOPT_TTYPE, SEND, (char)IAC, (char)SE, '\0' };
               const char *pStartPos = strstr( pClientName, "-" );

               /* Store the TTYPE */
               free(pProtocol->pLastTTYPE);
               pProtocol->pLastTTYPE = AllocString(pClientName);

               /* Look for 256 colour support */
               if ( pStartPos != NULL && MatchString(pStartPos, "-256color") )
               {
                  /* This is currently the only way to detect support for 256 
                   * colours in TinTin++, WinTin++ and BlowTorch.
                   */
                  pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
                  pProtocol->b256Support = eYES;
               }

               /* Request another TTYPE */
               if ( !bStopCyclicTTYPE )
                  Write(apDescriptor, RequestTTYPE);
            }

            if ( PrefixString("Mudlet", pClientName) )
            {
               /* Mudlet beta 15 and later supports 256 colours, but we can't 
                * identify it from the mud - everything prior to 1.1 claims 
                * to be version 1.0, so we just don't know.
                */ 
               pProtocol->b256Support = eSOMETIMES;

               if ( strlen(pClientName) > 7 )
               {
                  pClientName[6] = '\0';
                  free(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString);
                  pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pClientName);
                  free(pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString);
                  pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString = AllocString(pClientName+7);

                  /* Mudlet 1.1 and later supports 256 colours. */
                  if ( strcmp(pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString, "1.1") >= 0 )
                  {
                     pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
                     pProtocol->b256Support = eYES;
                  }
               }
            }
            else if ( MatchString(pClientName, "EMACS-RINZAI") )
            {
               /* We know for certain that this client has support */
               pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
               pProtocol->b256Support = eYES;
            }
            else if ( PrefixString("DecafMUD", pClientName) )
            {
               /* We know for certain that this client has support */
               pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt = 1;
               pProtocol->b256Support = eYES;

               if ( strlen(pClientName) > 9 )
               {
                  pClientName[8] = '\0';
                  free(pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString);
                  pProtocol->pVariables[eMSDP_CLIENT_ID]->pValueString = AllocString(pClientName);
                  free(pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString);
                  pProtocol->pVariables[eMSDP_CLIENT_VERSION]->pValueString = AllocString(pClientName+9);
               }
            }
            else if ( MatchString(pClientName, "MUSHCLIENT") || 
               MatchString(pClientName, "CMUD") || 
               MatchString(pClientName, "ATLANTIS") || 
               MatchString(pClientName, "KILDCLIENT") || 
               MatchString(pClientName, "TINTIN++") || 
               MatchString(pClientName, "TINYFUGUE") )
            {
               /* We know that some versions of this client have support */
               pProtocol->b256Support = eSOMETIMES;
            }
            else if ( MatchString(pClientName, "ZMUD") )
            {
               /* We know for certain that this client does not have support */
               pProtocol->b256Support = eNO;
            }
         }
         break;

      case (char)TELOPT_NAWS:
         if ( pProtocol->bNAWS )
         {
            /* Store the new width. */
            pProtocol->ScreenWidth = (unsigned char)apData[0];
            pProtocol->ScreenWidth <<= 8;
            pProtocol->ScreenWidth += (unsigned char)apData[1];

            /* Store the new height. */
            pProtocol->ScreenHeight = (unsigned char)apData[2];
            pProtocol->ScreenHeight <<= 8;
            pProtocol->ScreenHeight += (unsigned char)apData[3];
         }
         break;

      case (char)TELOPT_CHARSET:
         if ( pProtocol->bCHARSET )
         {
            /* Because we're only asking about UTF-8, we can just check the 
             * first character.  If you ask for more than one CHARSET you'll 
             * need to read through the results to see which are accepted.
             *
             * Note that the user must also use a unicode font!
             */
            if ( apData[0] == ACCEPTED )
               pProtocol->pVariables[eMSDP_UTF_8]->ValueInt = 1;
         }
         break;

      case (char)TELOPT_MSDP:
         if ( pProtocol->bMSDP )
         {
            ParseMSDP( apDescriptor, apData );
         }
         break;

      case (char)TELOPT_ATCP:
         if ( pProtocol->bATCP )
         {
            ParseATCP( apDescriptor, apData );
         }
         break;

		/*************** START GMCP ***************/
		case ( char ) TELOPT_GMCP:
			if ( pProtocol->bGMCP )
				ParseGMCP( apDescriptor, apData );
		break;
		/*************** END GMCP ***************/

      default: /* Unknown subnegotiation, so we simply ignore it. */
         break;
   }
}

static void SendNegotiationSequence( descriptor_t *apDescriptor, char aCmd, char aProtocol )
{
   char NegotiateSequence[4];

   NegotiateSequence[0] = (char)IAC;
   NegotiateSequence[1] = aCmd;
   NegotiateSequence[2] = aProtocol;
   NegotiateSequence[3] = '\0';

   Write(apDescriptor, NegotiateSequence);
}

static bool_t ConfirmNegotiation( descriptor_t *apDescriptor, negotiated_t aProtocol, bool_t abWillDo, bool_t abSendReply )
{
   bool_t bResult = false;

   if ( aProtocol >= eNEGOTIATED_TTYPE && aProtocol < eNEGOTIATED_MAX )
   {
      /* Only negotiate if the state has changed. */
      if ( apDescriptor->pProtocol->Negotiated[aProtocol] != abWillDo )
      {
         /* Store the new state. */
         apDescriptor->pProtocol->Negotiated[aProtocol] = abWillDo;

         bResult = true;

         if ( abSendReply )
         {
            switch ( aProtocol )
            {
               case eNEGOTIATED_TTYPE:
                  SendNegotiationSequence( apDescriptor, abWillDo ? DO : DONT, TELOPT_TTYPE );
                  break;
               case eNEGOTIATED_ECHO:
                  SendNegotiationSequence( apDescriptor, abWillDo ? WILL : WONT, TELOPT_ECHO );
                  break;
               case eNEGOTIATED_NAWS:
                  SendNegotiationSequence( apDescriptor, abWillDo ? DO : DONT, TELOPT_NAWS );
                  break;
               case eNEGOTIATED_CHARSET:
                  SendNegotiationSequence( apDescriptor, abWillDo ? DO : DONT, TELOPT_CHARSET );
                  break;
               case eNEGOTIATED_MSDP:
                  SendNegotiationSequence( apDescriptor, abWillDo ? WILL : WONT, TELOPT_MSDP );
                  break;
               case eNEGOTIATED_MSSP:
                  SendNegotiationSequence( apDescriptor, abWillDo ? WILL : WONT, TELOPT_MSSP );
                  break;
               case eNEGOTIATED_ATCP:
                  SendNegotiationSequence( apDescriptor, abWillDo ? DO : DONT, (char)TELOPT_ATCP );
                  break;
               case eNEGOTIATED_MSP:
                  SendNegotiationSequence( apDescriptor, abWillDo ? WILL : WONT, TELOPT_MSP );
                  break;
               case eNEGOTIATED_MXP:
                  SendNegotiationSequence( apDescriptor, abWillDo ? DO : DONT, TELOPT_MXP );
                  break;
               case eNEGOTIATED_MXP2:
                  SendNegotiationSequence( apDescriptor, abWillDo ? WILL : WONT, TELOPT_MXP );
                  break;
               case eNEGOTIATED_MCCP:
#ifdef USING_MCCP
                  SendNegotiationSequence( apDescriptor, abWillDo ? WILL : WONT, TELOPT_MCCP );
#endif /* USING_MCCP */
                  break;
               default:
                  bResult = false;
                  break;

				/*************** START GMCP ***************/
				case eNEGOTIATED_GMCP:
					SendNegotiationSequence( apDescriptor, abWillDo ? WILL : WONT, ( char ) TELOPT_GMCP );
				break;

				case eNEGOTIATED_SGA:
					SendNegotiationSequence( apDescriptor, abWillDo ? DO : DONT, ( char ) TELOPT_SGA );
				break;
				/*************** END GMCP ***************/
            }
         }
      }
   }

   return bResult;
}

/******************************************************************************
 Local MSDP functions.
 ******************************************************************************/

static void ParseMSDP( descriptor_t *apDescriptor, const char *apData )
{
   char Variable[MSDP_VAL][MAX_MSDP_SIZE+1] = { {'\0'}, {'\0'} };
   char *pPos = NULL, *pStart = NULL;

   while ( *apData )
   {
      switch ( *apData )
      {
         case MSDP_VAR: case MSDP_VAL:
            pPos = pStart = Variable[*apData++-1];
            break;
         default: /* Anything else */
            if ( pPos && pPos-pStart < MAX_MSDP_SIZE )
            {
               *pPos++ = *apData;
               *pPos = '\0';
            }

            if ( *++apData )
               continue;
      }

      ExecuteMSDPPair( apDescriptor, Variable[MSDP_VAR-1], Variable[MSDP_VAL-1] );
      Variable[MSDP_VAL-1][0] = '\0';
   }
}

static void ExecuteMSDPPair( descriptor_t *apDescriptor, const char *apVariable, const char *apValue )
{
   if ( apVariable[0] != '\0' && apValue[0] != '\0' )
   {
      if ( MatchString(apVariable, "SEND") )
      {
         bool_t bDone = false;
         int i; /* Loop counter */
         for ( i = eMSDP_NONE+1; i < eMSDP_MAX && !bDone; ++i )
         {
            if ( MatchString(apValue, VariableNameTable[i].pName) )
            {
               MSDPSend( apDescriptor, (variable_t)i );
               bDone = true;
            }
         }
      }
      else if ( MatchString(apVariable, "REPORT") )
      {
         bool_t bDone = false;
         int i; /* Loop counter */
         for ( i = eMSDP_NONE+1; i < eMSDP_MAX && !bDone; ++i )
         {
            if ( MatchString(apValue, VariableNameTable[i].pName) )
            {
               apDescriptor->pProtocol->pVariables[i]->bReport = true;
               apDescriptor->pProtocol->pVariables[i]->bDirty = true;
               bDone = true;
            }
         }
      }
      else if ( MatchString(apVariable, "RESET") )
      {
         if ( MatchString(apValue, "REPORTABLE_VARIABLES") || 
            MatchString(apValue, "REPORTED_VARIABLES") )
         {
            int i; /* Loop counter */
            for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
            {
               if ( apDescriptor->pProtocol->pVariables[i]->bReport )
               {
                  apDescriptor->pProtocol->pVariables[i]->bReport = false;
                  apDescriptor->pProtocol->pVariables[i]->bDirty = false;
               }
            }
         }
      }
      else if ( MatchString(apVariable, "UNREPORT") )
      {
         bool_t bDone = false;
         int i; /* Loop counter */
         for ( i = eMSDP_NONE+1; i < eMSDP_MAX && !bDone; ++i )
         {
            if ( MatchString(apValue, VariableNameTable[i].pName) )
            {
               apDescriptor->pProtocol->pVariables[i]->bReport = false;
               apDescriptor->pProtocol->pVariables[i]->bDirty = false;
               bDone = true;
            }
         }
      }
      else if ( MatchString(apVariable, "LIST") )
      {
         if ( MatchString(apValue, "COMMANDS") )
         {
            const char MSDPCommands[] = "LIST REPORT RESET SEND UNREPORT";
            MSDPSendList( apDescriptor, "COMMANDS", MSDPCommands );
         }
         else if ( MatchString(apValue, "LISTS") )
         {
            const char MSDPCommands[] = "COMMANDS LISTS CONFIGURABLE_VARIABLES REPORTABLE_VARIABLES REPORTED_VARIABLES SENDABLE_VARIABLES GUI_VARIABLES";
            MSDPSendList( apDescriptor, "LISTS", MSDPCommands );
         }
         /* Split this into two if some variables aren't REPORTABLE */
         else if ( MatchString(apValue, "SENDABLE_VARIABLES") || 
            MatchString(apValue, "REPORTABLE_VARIABLES") )
         {
            char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
            int i; /* Loop counter */

            for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
            {
               if ( !VariableNameTable[i].bGUI )
               {
                  /* Add the separator between variables */
                  strcat( MSDPCommands, " " );

                  /* Add the variable to the list */
                  strcat( MSDPCommands, VariableNameTable[i].pName );
               }
            }

            MSDPSendList( apDescriptor, apValue, MSDPCommands );
         }
         else if ( MatchString(apValue, "REPORTED_VARIABLES") )
         {
            char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
            int i; /* Loop counter */

            for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
            {
               if ( apDescriptor->pProtocol->pVariables[i]->bReport )
               {
                  /* Add the separator between variables */
                  if ( MSDPCommands[0] != '\0' )
                     strcat( MSDPCommands, " " );

                  /* Add the variable to the list */
                  strcat( MSDPCommands, VariableNameTable[i].pName );
               }
            }

            MSDPSendList( apDescriptor, apValue, MSDPCommands );
         }
         else if ( MatchString(apValue, "CONFIGURABLE_VARIABLES") )
         {
            char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
            int i; /* Loop counter */

            for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
            {
               if ( VariableNameTable[i].bConfigurable )
               {
                  /* Add the separator between variables */
                  if ( MSDPCommands[0] != '\0' )
                     strcat( MSDPCommands, " " );

                  /* Add the variable to the list */
                  strcat( MSDPCommands, VariableNameTable[i].pName );
               }
            }

            MSDPSendList( apDescriptor, "CONFIGURABLE_VARIABLES", MSDPCommands );
         }
         else if ( MatchString(apValue, "GUI_VARIABLES") )
         {
            char MSDPCommands[MAX_OUTPUT_BUFFER] = { '\0' };
            int i; /* Loop counter */

            for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
            {
               if ( VariableNameTable[i].bGUI )
               {
                  /* Add the separator between variables */
                  if ( MSDPCommands[0] != '\0' )
                     strcat( MSDPCommands, " " );

                  /* Add the variable to the list */
                  strcat( MSDPCommands, VariableNameTable[i].pName );
               }
            }

            MSDPSendList( apDescriptor, apValue, MSDPCommands );
         }
      }
      else /* Set any configurable variables */
      {
         int i; /* Loop counter */

         for ( i = eMSDP_NONE+1; i < eMSDP_MAX; ++i )
         {
            if ( VariableNameTable[i].bConfigurable )
            {
               if ( MatchString(apVariable, VariableNameTable[i].pName) )
               {
                  if ( VariableNameTable[i].bString )
                  {
                     /* A write-once variable can only be set if the value 
                      * is "Unknown".  This is for things like client name, 
                      * where we don't really want the player overwriting a 
                      * proper client name with junk - but on the other hand, 
                      * its possible a client may choose to use MSDP to 
                      * identify itself.
                      */
                     if ( !VariableNameTable[i].bWriteOnce || 
                        !strcmp(apDescriptor->pProtocol->pVariables[i]->pValueString, "Unknown") )
                     {
                        /* Store the new value if it's valid */
                        char *pBuffer = alloca(VariableNameTable[i].Max+1);
                        int j; /* Loop counter */

                        for ( j = 0; j < VariableNameTable[i].Max && *apValue != '\0'; ++apValue )
                        {
                           if ( isprint(*apValue) )
                              pBuffer[j++] = *apValue;
                        }
                        pBuffer[j++] = '\0';

                        if ( j >= VariableNameTable[i].Min )
                        {
                           free(apDescriptor->pProtocol->pVariables[i]->pValueString);
                           apDescriptor->pProtocol->pVariables[i]->pValueString = AllocString(pBuffer);
                        }
                     }
                  }
                  else /* This variable only accepts numeric values */
                  {
                     /* Strip any leading spaces */
                     while ( *apValue == ' ' )
                        ++apValue;

                     if ( *apValue != '\0' && IsNumber(apValue) )
                     {
                        int Value = atoi(apValue);
                        if ( Value >= VariableNameTable[i].Min && 
                           Value <= VariableNameTable[i].Max )
                        {
                           apDescriptor->pProtocol->pVariables[i]->ValueInt = Value;
                        }
                     }
                  }
               }
            }
         }
      }
   }
}

/******************************************************************************
 Local ATCP functions.
 ******************************************************************************/

static void ParseATCP( descriptor_t *apDescriptor, const char *apData )
{
   char Variable[MSDP_VAL][MAX_MSDP_SIZE+1] = { {'\0'}, {'\0'} };
   char *pPos = NULL, *pStart = NULL;

   while ( *apData )
   {
      switch ( *apData )
      {
         case '@': 
            pPos = pStart = Variable[0];
            apData++;
            break;
         case ' ': 
            pPos = pStart = Variable[1];
            apData++;
            break;
         default: /* Anything else */
            if ( pPos && pPos-pStart < MAX_MSDP_SIZE )
            {
               *pPos++ = *apData;
               *pPos = '\0';
            }

            if ( *++apData )
               continue;
      }

      ExecuteMSDPPair( apDescriptor, Variable[MSDP_VAR-1], Variable[MSDP_VAL-1] );
      Variable[MSDP_VAL-1][0] = '\0';
   }
}

#ifdef MUDLET_PACKAGE
static void SendATCP( descriptor_t *apDescriptor, const char *apVariable, const char *apValue )
{
   char ATCPBuffer[MAX_VARIABLE_LENGTH+1] = { '\0' };

   if ( apVariable != NULL && apValue != NULL )
   {
      protocol_t *pProtocol = apDescriptor ? apDescriptor->pProtocol : NULL;

      /* Should really be replaced with a dynamic buffer */
      int RequiredBuffer = strlen(apVariable) + strlen(apValue) + 12;

      if ( RequiredBuffer >= MAX_VARIABLE_LENGTH )
      {
         if ( RequiredBuffer - strlen(apValue) < MAX_VARIABLE_LENGTH )
         {
            sprintf( ATCPBuffer, 
               "SendATCP: %s %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", 
               apVariable, RequiredBuffer, MAX_VARIABLE_LENGTH );
         }
         else /* The variable name itself is too long */
         {
            sprintf( ATCPBuffer, 
               "SendATCP: Variable name has a length of %d bytes (exceeds MAX_VARIABLE_LENGTH of %d).\n", 
               RequiredBuffer, MAX_VARIABLE_LENGTH );
         }

         ReportBug( ATCPBuffer );
         ATCPBuffer[0] = '\0';
      }
      else if ( pProtocol->bATCP )
      {
         sprintf( ATCPBuffer, "%c%c%c%s %s%c%c", 
            IAC, SB, TELOPT_ATCP, apVariable, apValue, IAC, SE );
      }

      /* Just in case someone calls this function without checking ATCP */
      if ( ATCPBuffer[0] != '\0' )
         Write( apDescriptor, ATCPBuffer );
   }
}
#endif /* MUDLET_PACKAGE */

/******************************************************************************
 Local MSSP functions.
 ******************************************************************************/

static const char *GetMSSP_Players()
{
   static char Buffer[32];
   sprintf( Buffer, "%d", s_Players );
   return Buffer;
}

static const char *GetMSSP_Uptime()
{
   static char Buffer[32];
   sprintf( Buffer, "%d", (int)s_Uptime );
   return Buffer;
}

/* Macro for readability, but you can remove it if you don't like it */
#define FUNCTION_CALL(f) "", f

static void SendMSSP( descriptor_t *apDescriptor )
{
   char MSSPBuffer[MAX_MSSP_BUFFER];
   char MSSPPair[128];
   int SizeBuffer = 3; /* IAC SB MSSP */
   int i; /* Loop counter */

   /* Before updating the following table, please read the MSSP specification:
    *
    * http://tintin.sourceforge.net/mssp/
    *
    * It's important that you use the correct format and spelling for the MSSP 
    * variables, otherwise crawlers may reject the data as invalid.
    */
   static MSSP_t MSSPTable[] =
   {
      /* Required */
      { "NAME",               MUD_NAME },   /* Change this in protocol.h */
      { "PLAYERS",            FUNCTION_CALL( GetMSSP_Players ) },
      { "UPTIME" ,            FUNCTION_CALL( GetMSSP_Uptime ) }, 

      /* Generic */
      { "CRAWL DELAY",        "-1" },
/*
      { "HOSTNAME",           "" },
      { "PORT",               "" },
      { "CODEBASE",           "" },
      { "CONTACT",            "" },
      { "CREATED",            "" },
      { "ICON",               "" },
      { "IP",                 "" },
      { "LANGUAGE",           "" },
      { "LOCATION",           "" },
      { "MINIMUM AGE",        "" },
      { "WEBSITE",            "" },
*/
      /* Categorisation */
/*
      { "FAMILY",             "" },
      { "GENRE",              "" },
      { "GAMEPLAY",           "" },
      { "STATUS",             "" },
      { "GAMESYSTEM",         "" },
      { "INTERMUD",           "" },
      { "SUBGENRE",           "" },
*/
      /* World */
/*
      { "AREAS",              "0" },
      { "HELPFILES",          "0" },
      { "MOBILES",            "0" },
      { "OBJECTS",            "0" },
      { "ROOMS",              "0" },
      { "CLASSES",            "0" },
      { "LEVELS",             "0" },
      { "RACES",              "0" },
      { "SKILLS",             "0" },
*/
      /* Protocols */
/*
      { "ANSI",               "1" },
      { "GMCP",               "0" },
#ifdef USING_MCCP
      { "MCCP",               "1" },
#else
      { "MCCP",               "0" },
#endif // USING_MCCP
      { "MCP",                "0" },
      { "MSDP",               "1" },
      { "MSP",                "1" },
      { "MXP",                "1" },
      { "PUEBLO",             "0" },
      { "UTF-8",              "1" },
      { "VT100",              "0" },
      { "XTERM 256 COLORS",   "1" },
*/
      /* Commercial */
/*
      { "PAY TO PLAY",        "0" },
      { "PAY FOR PERKS",      "0" },
*/
      /* Hiring */
/*
      { "HIRING BUILDERS",    "0" },
      { "HIRING CODERS",      "0" },
*/
      /* Extended variables */

      /* World */
/*
      { "DBSIZE",             "0" },
      { "EXITS",              "0" },
      { "EXTRA DESCRIPTIONS", "0" },
      { "MUDPROGS",           "0" },
      { "MUDTRIGS",           "0" },
      { "RESETS",             "0" },
*/
      /* Game */
/*
      { "ADULT MATERIAL",     "0" },
      { "MULTICLASSING",      "0" },
      { "NEWBIE FRIENDLY",    "0" },
      { "PLAYER CITIES",      "0" },
      { "PLAYER CLANS",       "0" },
      { "PLAYER CRAFTING",    "0" },
      { "PLAYER GUILDS",      "0" },
      { "EQUIPMENT SYSTEM",   "" },
      { "MULTIPLAYING",       "" },
      { "PLAYERKILLING",      "" },
      { "QUEST SYSTEM",       "" },
      { "ROLEPLAYING",        "" },
      { "TRAINING SYSTEM",    "" },
      { "WORLD ORIGINALITY",  "" },
*/
      /* Protocols */
/*
      { "ATCP",               "1" },
      { "SSL",                "0" },
      { "ZMP",                "0" },
*/
      { NULL, NULL } /* This must always be last. */
   };

   /* Begin the subnegotiation sequence */
   sprintf( MSSPBuffer, "%c%c%c", IAC, SB, TELOPT_MSSP );

   for ( i = 0; MSSPTable[i].pName != NULL; ++i )
   {
      int SizePair;

      /* Retrieve the next MSSP variable/value pair */
      sprintf( MSSPPair, "%c%s%c%s", MSSP_VAR, MSSPTable[i].pName, MSSP_VAL, 
         MSSPTable[i].pFunction ? (*MSSPTable[i].pFunction)() : 
         MSSPTable[i].pValue );

      /* Make sure we don't overflow the buffer */
      SizePair = strlen(MSSPPair);
      if ( SizePair+SizeBuffer < MAX_MSSP_BUFFER-4 )
      {
         strcat( MSSPBuffer, MSSPPair );
         SizeBuffer += SizePair;
      }
   }

   /* End the subnegotiation sequence */
   sprintf( MSSPPair, "%c%c", IAC, SE );
   strcat( MSSPBuffer, MSSPPair );

   /* Send the sequence */
   Write( apDescriptor, MSSPBuffer );
}

/******************************************************************************
 Local MXP functions.
 ******************************************************************************/

static char *GetMxpTag( const char *apTag, const char *apText )
{
   static char MXPBuffer [64];
   const char *pStartPos = strstr(apText, apTag);

   if ( pStartPos != NULL )
   {
      const char *pEndPos = apText+strlen(apText);

      pStartPos += strlen(apTag); /* Add length of the tag */

      if ( pStartPos < pEndPos )
      {
         int Index = 0;

         /* Some clients use quotes...and some don't. */
         if ( *pStartPos == '\"' )
            pStartPos++;

         for ( ; pStartPos < pEndPos && Index < 60; ++pStartPos )
         {
            char Letter = *pStartPos;
            if ( Letter == '.' || isdigit(Letter) || isalpha(Letter) )
            {
               MXPBuffer[Index++] = Letter;
            }
            else /* Return the result */
            {
               MXPBuffer[Index] = '\0';
               return MXPBuffer;
            }
         }
      }
   }

   /* Return NULL to indicate no tag was found. */
   return NULL;
}

/******************************************************************************
 Local colour functions.
 ******************************************************************************/

static const char *GetAnsiColour( bool_t abBackground, int aRed, int aGreen, int aBlue )
{
   if ( aRed == aGreen && aRed == aBlue && aRed < 2)
      return abBackground ? s_BackBlack : aRed >= 1 ? s_BoldBlack : s_DarkBlack;
   else if ( aRed == aGreen && aRed == aBlue )
      return abBackground ? s_BackWhite : aRed >= 4 ? s_BoldWhite : s_DarkWhite;
   else if ( aRed > aGreen && aRed > aBlue )
      return abBackground ? s_BackRed : aRed >= 3 ? s_BoldRed : s_DarkRed;
   else if ( aRed == aGreen && aRed > aBlue )
      return abBackground ? s_BackYellow : aRed >= 3 ? s_BoldYellow : s_DarkYellow;
   else if ( aRed == aBlue && aRed > aGreen )
      return abBackground ? s_BackMagenta : aRed >= 3 ? s_BoldMagenta : s_DarkMagenta;
   else if ( aGreen > aBlue )
      return abBackground ? s_BackGreen : aGreen >= 3 ? s_BoldGreen : s_DarkGreen;
   else if ( aGreen == aBlue )
      return abBackground ? s_BackCyan : aGreen >= 3 ? s_BoldCyan : s_DarkCyan;
   else /* aBlue is the highest */
      return abBackground ? s_BackBlue : aBlue >= 3 ? s_BoldBlue : s_DarkBlue;
}

static const char *GetRGBColour( bool_t abBackground, int aRed, int aGreen, int aBlue )
{
   static char Result[16];
   int ColVal = 16 + (aRed * 36) + (aGreen * 6) + aBlue;
   sprintf( Result, "\033[%c8;5;%c%c%cm", 
      '3'+abBackground,      /* Background */
      '0'+(ColVal/100),      /* Red        */
      '0'+((ColVal%100)/10), /* Green      */
      '0'+(ColVal%10) );     /* Blue       */
   return Result;
}

static bool_t IsValidColour( const char *apArgument )
{
   int i; /* Loop counter */

   /* The sequence is 4 bytes, but we can ignore anything after it. */
   if ( apArgument == NULL || strlen(apArgument) < 4 )
      return false;

   /* The first byte indicates foreground/background. */
   if ( tolower(apArgument[0]) != 'f' && tolower(apArgument[0]) != 'b' )
      return false;

   /* The remaining three bytes must each be in the range '0' to '5'. */
   for ( i = 1; i <= 3; ++i )
   {
      if ( apArgument[i] < '0' || apArgument[i] > '5' )
         return false;
   }

   /* It's a valid foreground or background colour */
   return true;
}

/******************************************************************************
 Other local functions.
 ******************************************************************************/

static bool_t MatchString( const char *apFirst, const char *apSecond )
{
   while ( *apFirst && tolower(*apFirst) == tolower(*apSecond) )
      ++apFirst, ++apSecond;
   return ( !*apFirst && !*apSecond );
}

static bool_t PrefixString( const char *apPart, const char *apWhole )
{
   while ( *apPart && tolower(*apPart) == tolower(*apWhole) )
      ++apPart, ++apWhole;
   return ( !*apPart );
}

static bool_t IsNumber( const char *apString )
{
   while ( *apString && isdigit(*apString) )
      ++apString;
   return ( !*apString );
}

static char *AllocString( const char *apString )
{
   char *pResult = NULL;

   if ( apString != NULL )
   {
      int Size = strlen(apString);
      pResult = malloc(Size+1);
      if ( pResult != NULL )
         strcpy( pResult, apString );
   }

   return pResult;
}

/*************** START GMCP ***************/
const char GoAheadStr[] = { IAC, GA, '\0' };
const char iac_sb_gmcp[] = { IAC, SB, TELOPT_GMCP, '\0' };
const char iac_se[] = { IAC, SE, '\0' };

const struct gmcp_receive_struct GMCPReceiveTable[GMCP_RECEIVE_MAX+1] =
{
	{ GMCP_CORE_HELLO,					"Core.Hello"						},
	{ GMCP_CORE_SUPPORTS_SET,			"Core.Supports.Set"					},
	{ GMCP_CORE_SUPPORTS_ADD,			"Core.Supports.Add"					},
	{ GMCP_CORE_SUPPORTS_REMOVE,		"Core.Supports.Remove"				},
	{ GMCP_EXTERNAL_DISCORD_HELLO,		"External.Discord.Hello"			},
	{ GMCP_EXTERNAL_DISCORD_GET,		"External.Discord.Get"				},

	{ GMCP_RECEIVE_MAX,					"",									}
};

const struct gmcp_package_struct GMCPPackageTable[GMCP_PACKAGE_MAX+1] =
{
	{ GMCP_BASE,					GMCP_SUPPORT_CHAR,			"Char",			"Base"		},
	{ GMCP_VITALS,					GMCP_SUPPORT_CHAR,			"Char",			"Vitals"	},
	{ GMCP_STATS,					GMCP_SUPPORT_CHAR,			"Char",			"Stats"		},
	{ GMCP_AC,						GMCP_SUPPORT_CHAR,			"Char",			"AC"		},
	{ GMCP_WORTH,					GMCP_SUPPORT_CHAR,			"Char",			"Worth"		},
	{ GMCP_AFFECTED,				GMCP_SUPPORT_CHAR,			"Char",			"Affect"	},
	{ GMCP_ENEMIES,					GMCP_SUPPORT_CHAR,			"Char",			"Enemies"	},
	{ GMCP_ROOM,					GMCP_SUPPORT_ROOM,			"Room",			"Info"		},

	{ GMCP_PACKAGE_MAX,				-1,							"",				""			}
};

const struct gmcp_support_struct bGMCPSupportTable[GMCP_SUPPORT_MAX+1] =
{
	{ GMCP_SUPPORT_CHAR,			"Char"						},
	{ GMCP_SUPPORT_ROOM,			"Room"						},

	{ GMCP_SUPPORT_MAX,				NULL						}
};

const struct gmcp_variable_struct GMCPVariableTable[GMCP_MAX+1] =
{
	{ GMCP_CLIENT,		-1,					"client",		GMCP_STRING	},
	{ GMCP_VERSION,		-1,					"version",		GMCP_STRING	},

	{ GMCP_NAME,		GMCP_BASE,			"name",			GMCP_STRING	},
	{ GMCP_RACE,		GMCP_BASE,			"race",			GMCP_STRING	},
	{ GMCP_CLASS,		GMCP_BASE,			"class",		GMCP_STRING	},

	{ GMCP_HP,			GMCP_VITALS,		"hp",			GMCP_NUMBER	},
	{ GMCP_MANA,		GMCP_VITALS,		"mana",			GMCP_NUMBER	},
	{ GMCP_MOVE,		GMCP_VITALS,		"move",			GMCP_NUMBER	},
	{ GMCP_MAX_HP,		GMCP_VITALS,		"maxhp",		GMCP_NUMBER	},
	{ GMCP_MAX_MANA,	GMCP_VITALS,		"maxmana",		GMCP_NUMBER	},
	{ GMCP_MAX_MOVE,	GMCP_VITALS,		"maxmove",		GMCP_NUMBER	},

	{ GMCP_STR,			GMCP_STATS,			"str",			GMCP_NUMBER	},
	{ GMCP_INT,			GMCP_STATS,			"int",			GMCP_NUMBER	},
	{ GMCP_WIS,			GMCP_STATS,			"wis",			GMCP_NUMBER	},
	{ GMCP_DEX,			GMCP_STATS,			"dex",			GMCP_NUMBER	},
	{ GMCP_CON,			GMCP_STATS,			"con",			GMCP_NUMBER	},
	{ GMCP_HITROLL,		GMCP_STATS,			"hitroll",		GMCP_NUMBER	},
	{ GMCP_DAMROLL,		GMCP_STATS,			"damroll",		GMCP_NUMBER	},
	{ GMCP_STR_PERM,	GMCP_STATS,			"permstr",		GMCP_NUMBER	},
	{ GMCP_INT_PERM,	GMCP_STATS,			"permint",		GMCP_NUMBER	},
	{ GMCP_WIS_PERM,	GMCP_STATS,			"permwis",		GMCP_NUMBER	},
	{ GMCP_DEX_PERM,	GMCP_STATS,			"permdex",		GMCP_NUMBER	},
	{ GMCP_CON_PERM,	GMCP_STATS,			"permcon",		GMCP_NUMBER	},
	{ GMCP_WIMPY,		GMCP_STATS,			"wimpy",		GMCP_NUMBER	},

	{ GMCP_AC_PIERCE,	GMCP_AC,			"pierce",		GMCP_NUMBER	},
	{ GMCP_AC_BASH,		GMCP_AC,			"bash",			GMCP_NUMBER	},
	{ GMCP_AC_SLASH,	GMCP_AC,			"slash",		GMCP_NUMBER	},
	{ GMCP_AC_EXOTIC,	GMCP_AC,			"exotic",		GMCP_NUMBER	},

	{ GMCP_ALIGNMENT,	GMCP_WORTH,			"alignment",	GMCP_NUMBER	},
	{ GMCP_XP,			GMCP_WORTH,			"xp",			GMCP_NUMBER	},
	{ GMCP_XP_MAX,		GMCP_WORTH,			"maxxp",		GMCP_NUMBER	},
	{ GMCP_XP_TNL,		GMCP_WORTH,			"xptnl",		GMCP_NUMBER	},
	{ GMCP_PRACTICE,	GMCP_WORTH,			"practice",		GMCP_NUMBER	},
	{ GMCP_MONEY,		GMCP_WORTH,			"money",		GMCP_NUMBER	},

	{ GMCP_ENEMY,		GMCP_ENEMIES,		NULL,			GMCP_ARRAY	},

	{ GMCP_AFFECT,		GMCP_AFFECTED,		NULL,			GMCP_ARRAY	},

	{ GMCP_AREA,		GMCP_ROOM,			"area",			GMCP_STRING	},
	{ GMCP_ROOM_NAME,	GMCP_ROOM,			"name",			GMCP_STRING	},
	{ GMCP_ROOM_EXITS,	GMCP_ROOM,			"exit",			GMCP_OBJECT	},
	{ GMCP_ROOM_VNUM,	GMCP_ROOM,			"vnum",			GMCP_NUMBER	},

	{ GMCP_MAX,			-1,					"",				GMCP_NUMBER	}
};

/******************************************************************************
 JSMN (JSON) functions.
 ******************************************************************************/

static jsmntok_t *jsmn_alloc_token( jsmn_parser *parser, jsmntok_t *tokens, const size_t num_tokens )
{
	jsmntok_t *tok;

	if ( parser->toknext >= num_tokens )
		return NULL;

	tok = &tokens[parser->toknext++];
	tok->start = tok->end = -1;
	tok->size = 0;

	return tok;
}

static void jsmn_fill_token( jsmntok_t *token, const jsmntype_t type, const int start, const int end )
{
	token->type = type;
	token->start = start;
	token->end = end;
	token->size = 0;

	return;
}

static int jsmn_parse_primitive( jsmn_parser *parser, const char *js, const size_t len, jsmntok_t *tokens, const size_t num_tokens )
{
	jsmntok_t *token;
	int start;

	start = parser->pos;

	for ( ; parser->pos < len && js[parser->pos] != '\0'; parser->pos++ )
	{
		switch ( js[parser->pos] )
		{
			case '\t':
			case '\r':
			case '\n':
			case ' ':
			case ',':
			case ']':
			case '}':
				goto found;

			default: break; /* to quiet a warning from gcc */
		}

		if ( js[parser->pos] < 32 || js[parser->pos] >= 127 )
		{
			parser->pos = start;

			return JSMN_ERROR_INVAL;
		}
	}

	found:
	if ( tokens == NULL )
	{
		parser->pos--;
		return 0;
	}

	token = jsmn_alloc_token(parser, tokens, num_tokens);

	if ( token == NULL )
	{
		parser->pos = start;
		return JSMN_ERROR_NOMEM;
	}

	jsmn_fill_token( token, JSMN_PRIMITIVE, start, parser->pos );
	parser->pos--;

	return 0;
}

static int jsmn_parse_string( jsmn_parser *parser, const char *js, const size_t len, jsmntok_t *tokens, const size_t num_tokens )
{
	jsmntok_t *token;
	int start = parser->pos;

	parser->pos++;

	/* Skip starting quote */
	for ( ; parser->pos < len && js[parser->pos] != '\0'; parser->pos++ )
	{
		char c = js[parser->pos];

		/* Quote: end of string */
		if ( c == '\"' )
		{
			if ( tokens == NULL )
			{
				return 0;
			}

			token = jsmn_alloc_token(parser, tokens, num_tokens);

			if ( token == NULL )
			{
				parser->pos = start;
				return JSMN_ERROR_NOMEM;
			}

			jsmn_fill_token(token, JSMN_STRING, start + 1, parser->pos);

			return 0;
		}

		/* Backslash: Quoted symbol expected */
		if ( c == '\\' && parser->pos + 1 < len )
		{
			int i;
			parser->pos++;

			switch ( js[parser->pos] )
			{
				/* Allowed escaped symbols */
				case '\"':
				case '/':
				case '\\':
				case 'b':
				case 'f':
				case 'r':
				case 'n':
				case 't':
				break;

				/* Allows escaped symbol \uXXXX */
				case 'u':
					parser->pos++;

					for ( i = 0; i < 4 && parser->pos < len && js[parser->pos] != '\0'; i++ )
					{
						/* If it isn't a hex character we have an error */
						if ( !( ( js[parser->pos] >= 48 && js[parser->pos] <= 57 ) ||   /* 0-9 */
								( js[parser->pos] >= 65 && js[parser->pos] <= 70 ) ||	/* A-F */
								( js[parser->pos] >= 97 && js[parser->pos] <= 102 ) ) ) /* a-f */
						{
							parser->pos = start;
							return JSMN_ERROR_INVAL;
						}

						parser->pos++;
					}

					parser->pos--;
				break;

				/* Unexpected symbol */
				default:
					parser->pos = start;
					return JSMN_ERROR_INVAL;
			}
		}
	}

	parser->pos = start;

	return JSMN_ERROR_PART;
}

int jsmn_parse( jsmn_parser *parser, const char *js, const size_t len, jsmntok_t *tokens, const unsigned int num_tokens )
{
	int r, i, count = parser->toknext;
	jsmntok_t *token;

	for ( ; parser->pos < len && js[parser->pos] != '\0'; parser->pos++ )
	{
		char c;
		jsmntype_t type;

		c = js[parser->pos];

		switch ( c )
		{
			case '{':
			case '[':
				count++;
				if ( tokens == NULL )
				{
					break;
				}

				token = jsmn_alloc_token(parser, tokens, num_tokens);

				if ( token == NULL )
				{
					return JSMN_ERROR_NOMEM;
				}

				if ( parser->toksuper != -1 )
				{
					jsmntok_t *t = &tokens[parser->toksuper];
					t->size++;
				}

				token->type = ( c == '{' ? JSMN_OBJECT : JSMN_ARRAY );
				token->start = parser->pos;
				parser->toksuper = parser->toknext - 1;
			break;

			case '}':
			case ']':
				if ( tokens == NULL )
				{
					break;
				}

				type = ( c == '}' ? JSMN_OBJECT : JSMN_ARRAY );

				for ( i = parser->toknext - 1; i >= 0; i-- )
				{
					token = &tokens[i];

					if ( token->start != -1 && token->end == -1 )
					{
						if ( token->type != type )
						{
							return JSMN_ERROR_INVAL;
						}

						parser->toksuper = -1;
						token->end = parser->pos + 1;
						break;
					}
				}

				/* Error if unmatched closing bracket */
				if ( i == -1 )
				{
					return JSMN_ERROR_INVAL;
				}

				for ( ; i >= 0; i-- )
				{
					token = &tokens[i];
					if ( token->start != -1 && token->end == -1 )
					{
						parser->toksuper = i;
						break;
					}
				}
			break;

			case '\"':
				r = jsmn_parse_string( parser, js, len, tokens, num_tokens );
				if ( r < 0 )
				{
					return r;
				}
				count++;

				if ( parser->toksuper != -1 && tokens != NULL )
				{
					tokens[parser->toksuper].size++;
				}
			break;

			case '\t':
			case '\r':
			case '\n':
			case ' ':
			break;

			case ':':
				parser->toksuper = parser->toknext - 1;
			break;

			case ',':
				if ( tokens != NULL && parser->toksuper != -1 &&
					tokens[parser->toksuper].type != JSMN_ARRAY &&
					tokens[parser->toksuper].type != JSMN_OBJECT )
				{
					for ( i = parser->toknext - 1; i >= 0; i-- )
					{
						if ( tokens[i].type == JSMN_ARRAY || tokens[i].type == JSMN_OBJECT )
						{
							if ( tokens[i].start != -1 && tokens[i].end == -1 )
							{
								parser->toksuper = i;
								break;
							}
						}
					}
				}
			break;

			/* In non-strict mode every unquoted value is a primitive */
			default:
				r = jsmn_parse_primitive( parser, js, len, tokens, num_tokens );
				if ( r < 0 )
				{
					return r;
				}
				count++;

				if ( parser->toksuper != -1 && tokens != NULL )
				{
					tokens[parser->toksuper].size++;
				}
			break;
		}
	}

	if ( tokens != NULL )
	{
		for ( i = parser->toknext - 1; i >= 0; i-- )
		{
			/* Unmatched opened object or array */
			if ( tokens[i].start != -1 && tokens[i].end == -1 )
			{
				return JSMN_ERROR_PART;
			}
		}
	}

	return count;
}

void jsmn_init( jsmn_parser *parser )
{
	parser->pos = 0;
	parser->toknext = 0;
	parser->toksuper = -1;

	return;
}

char *PullJSONString( int start, int end, const char *string )
{
	static char buf[MAX_PROTOCOL_BUFFER];
	int i, p = 0;

	buf[p] = '\0';

	for ( i = start; i < end; i++ )
	{
		buf[p++] = string[i];
	}

	buf[p] = '\0';

	return buf;
}

void ParseGMCP( descriptor_t *apDescriptor, char *string )
{
	int i, tokens = 0;
	jsmn_parser p;
	jsmntok_t t[24];
	char *module, *key;

	jsmn_init( &p );
	if ( ( tokens = jsmn_parse( &p, string, strlen(string), t, 24 ) ) < 1 )
		return;

	module = PullJSONString( t[0].start, t[0].end, string );

	for ( i = 0; GMCPReceiveTable[i].module != GMCP_RECEIVE_MAX; i++ )
	{
		if ( !strcmp( module, GMCPReceiveTable[i].string ) )
			break;
	}

	if ( GMCPReceiveTable[i].module == GMCP_RECEIVE_MAX )
		return; /* No module found with this string */

	switch ( GMCPReceiveTable[i].module )
	{
		default: break;

		case GMCP_CORE_HELLO:
			if ( t[1].type == JSMN_OBJECT )
			{
				for ( i = 2; i < tokens; i++ )
				{
					key = PullJSONString( t[i].start, t[i].end, string );

					if ( !strcmp( key, "client" ) )
					{
						i++;
						free( apDescriptor->pProtocol->GMCPVariable[GMCP_CLIENT] );
						apDescriptor->pProtocol->GMCPVariable[GMCP_CLIENT] = AllocString( PullJSONString( t[i].start, t[i].end, string ) );
					}
					else if ( !strcmp( key, "version" ) )
					{
						i++;
						free( apDescriptor->pProtocol->GMCPVariable[GMCP_VERSION] );
						apDescriptor->pProtocol->GMCPVariable[GMCP_VERSION] = AllocString( PullJSONString( t[i].start, t[i].end, string ) );
					}
					else
						break;
				}
			}
		break;

		case GMCP_CORE_SUPPORTS_SET:
		{
			char buf[MAX_PROTOCOL_BUFFER];
			char toggle = 0;
			int j;

			for ( i = 2; i < t[1].size + 2; i++ )
			{
				module = PullJSONString( t[i].start, t[i].end, string );
				module = OneArg( module, buf );
				toggle = atoi( module );

				for ( j = 0; bGMCPSupportTable[j].module != GMCP_SUPPORT_MAX; j++ )
				{
					if ( !strcmp( buf, bGMCPSupportTable[j].name ) )
					{
						apDescriptor->pProtocol->bGMCPSupport[j] = toggle;
						break;
					}
				}
			}
		}
		break;

		case GMCP_CORE_SUPPORTS_ADD:
		{
			char buf[MAX_PROTOCOL_BUFFER];
			char toggle = 0;
			int j;

			for ( i = 2; i < t[1].size + 2; i++ )
			{
				module = PullJSONString( t[i].start, t[i].end, string );
				module = OneArg( module, buf );
				toggle = atoi( module );

				for ( j = 0; bGMCPSupportTable[j].module != GMCP_SUPPORT_MAX; j++ )
				{
					if ( !strcmp( buf, bGMCPSupportTable[j].name ) )
					{
						apDescriptor->pProtocol->bGMCPSupport[j] = toggle;
						break;
					}
				}
			}
		}
		break;

		case GMCP_CORE_SUPPORTS_REMOVE:
		{
			char buf[MAX_PROTOCOL_BUFFER];
			int j;

			for ( i = 2; i < t[1].size + 2; i++ )
			{
				module = PullJSONString( t[i].start, t[i].end, string );

				for ( j = 0; bGMCPSupportTable[j].module != GMCP_SUPPORT_MAX; j++ )
				{
					if ( !strcmp( buf, bGMCPSupportTable[j].name ) )
					{
						apDescriptor->pProtocol->bGMCPSupport[j] = false;
						break;
					}
				}
			}
		}
		break;

		case GMCP_EXTERNAL_DISCORD_HELLO:
		{
			char buf[MAX_PROTOCOL_BUFFER];

			#ifndef COLOR_CODE_FIX
			sprintf( buf, "%sExternal.Discord.Info { \"inviteurl\": \"%s\", \"applicationid\": \"%s\" }%s",
			( char * ) iac_sb_gmcp,
			DISCORD_INVITE_URL,
			DISCORD_APPLICACTION_ID,
			( char * ) iac_se );
			#else
			sprintf( buf, "%sExternal.Discord.Info {{ \"inviteurl\": \"%s\", \"applicationid\": \"%s\" }%s",
			( char * ) iac_sb_gmcp,
			DISCORD_INVITE_URL,
			DISCORD_APPLICACTION_ID,
			( char * ) iac_se );
			#endif
			Write( apDescriptor, buf );
		}
		break;

		case GMCP_EXTERNAL_DISCORD_GET:
		{
			char buf[MAX_PROTOCOL_BUFFER];

			#ifndef COLOR_CODE_FIX
			sprintf( buf, "%sExternal.Discord.Status { "
			"\"smallimage\": [\"%s\", \"%s\", \"%s\"], "
			"\"smallimagetext\": \"%s\", "
			"\"details\": \"%s\", "
			"\"state\": \"%s\", "
			"\"game\": \"%s\" "
			"}%s",
			( char * ) iac_sb_gmcp,
			DISCORD_ICON_1, DISCORD_ICON_2, DISCORD_ICON_3,
			DISCORD_SMALL_IMAGE_TEXT,
			DISCORD_DETAILS,
			DISCORD_STATE,
			MUD_NAME,
			( char * ) iac_se );
			#else
			#endif
			Write( apDescriptor, buf );
		}
		break;
	}

	return;
}

void WriteGMCP( descriptor_t *apDescriptor, GMCP_PACKAGE package )
{
	char buf[MAX_PROTOCOL_BUFFER], buf2[MAX_PROTOCOL_BUFFER];
	int i, first = 0;

	#ifndef COLOR_CODE_FIX
	sprintf( buf, "%s%s.%s { ", ( char * ) iac_sb_gmcp, GMCPPackageTable[package].module, GMCPPackageTable[package].message );
	#else
	sprintf( buf, "%s%s.%s {{ ", ( char * ) iac_sb_gmcp, GMCPPackageTable[package].module, GMCPPackageTable[package].message );
	#endif

	for ( i = 0; GMCPVariableTable[i].variable != GMCP_MAX; i++ )
	{
		if ( GMCPVariableTable[i].package != package )
			continue;

		if ( !first )
		{
			first++;
			switch ( GMCPVariableTable[i].type )
			{
				case GMCP_STRING: sprintf( buf2, "\"%s\": \"%s\"", GMCPVariableTable[i].name, GMCPStrip( apDescriptor->pProtocol->GMCPVariable[i] ) );
				break;

				case GMCP_NUMBER: sprintf( buf2, "\"%s\": \"%d\"", GMCPVariableTable[i].name, atoi( apDescriptor->pProtocol->GMCPVariable[i] ) );
				break;

				#ifndef COLOR_CODE_FIX
				case GMCP_OBJECT: sprintf( buf2, ", \"%s\": { %s }", GMCPVariableTable[i].name, apDescriptor->pProtocol->GMCPVariable[i] );
				break;
				#else
				case GMCP_OBJECT: sprintf( buf2, ", \"%s\": {{ %s }", GMCPVariableTable[i].name, apDescriptor->pProtocol->GMCPVariable[i] );
				break;
				#endif

				case GMCP_ARRAY:
					sprintf( buf, "%s%s.%s [ %s ]%s", ( char * ) iac_sb_gmcp, GMCPPackageTable[package].module, GMCPPackageTable[package].message, apDescriptor->pProtocol->GMCPVariable[i], ( char * ) iac_se );
					Write( apDescriptor, buf );
					return;
				break;

				default: break;
			}
		}
		else
		{
			switch ( GMCPVariableTable[i].type )
			{
				case GMCP_STRING: sprintf( buf2, ", \"%s\": \"%s\"", GMCPVariableTable[i].name, GMCPStrip( apDescriptor->pProtocol->GMCPVariable[i] ) );
				break;

				case GMCP_NUMBER: sprintf( buf2, ", \"%s\": \"%d\"", GMCPVariableTable[i].name, atoi( apDescriptor->pProtocol->GMCPVariable[i] ) );
				break;

				#ifndef COLOR_CODE_FIX
				case GMCP_OBJECT: sprintf( buf2, ", \"%s\": { %s }", GMCPVariableTable[i].name, apDescriptor->pProtocol->GMCPVariable[i] );
				break;
				#else
				case GMCP_OBJECT: sprintf( buf2, ", \"%s\": {{ %s }", GMCPVariableTable[i].name, apDescriptor->pProtocol->GMCPVariable[i] );
				break;
				#endif

				default: break;
			}
		}

		strcat( buf, buf2 );
	}

	sprintf( buf2, " }%s", ( char * ) iac_se );
	strcat( buf, buf2 );

	Write( apDescriptor, buf );

	return;
}

void SendUpdatedGMCP( descriptor_t *apDescriptor )
{
	int i;

	if ( !apDescriptor || !apDescriptor->pProtocol || !apDescriptor->pProtocol->bGMCP )
		return;

	for ( i = 0; i < GMCP_PACKAGE_MAX; i++ )
	{
		if ( apDescriptor->pProtocol->bGMCPSupport[GMCPPackageTable[i].support] == 0 )
			continue;

		if ( apDescriptor->pProtocol->bGMCPUpdatePackage[i] )
			WriteGMCP( apDescriptor, i );

		apDescriptor->pProtocol->bGMCPUpdatePackage[i] = 0;
	}

	return;
}

void UpdateGMCPString( descriptor_t *apDescriptor, GMCP_VARIABLE var, const char *string )
{
	if ( !apDescriptor || !apDescriptor->pProtocol )
		return;

	if ( var < 0 || var >= GMCP_MAX )
		return;

	if ( !strcmp( apDescriptor->pProtocol->GMCPVariable[var], string ) )
		return;

	free( apDescriptor->pProtocol->GMCPVariable[var] );
	apDescriptor->pProtocol->GMCPVariable[var] = AllocString( string );
	apDescriptor->pProtocol->bGMCPUpdatePackage[GMCPVariableTable[var].package] = 1;

	return;
}

void UpdateGMCPNumber( descriptor_t *apDescriptor, GMCP_VARIABLE var, const long long number )
{
	char buf[MAX_PROTOCOL_BUFFER];

	if ( !apDescriptor || !apDescriptor->pProtocol )
		return;

	if ( var < 0 || var >= GMCP_MAX )
		return;

	sprintf( buf, "%lld", number );

	if ( !strcmp( apDescriptor->pProtocol->GMCPVariable[var], buf ) )
		return;

	free( apDescriptor->pProtocol->GMCPVariable[var] );
	apDescriptor->pProtocol->GMCPVariable[var] = AllocString( buf );
	apDescriptor->pProtocol->bGMCPUpdatePackage[GMCPVariableTable[var].package] = 1;

	return;
}

static char *OneArg( char *fStr, char *bStr )
{
	while ( isspace( *fStr ) )
		fStr++;

	char argEnd = ' ';

	if( *fStr == '\'')
	{
		argEnd = *fStr;
		fStr++;
	}

	while ( *fStr != '\0' )
	{
		if ( *fStr == argEnd )
		{
			fStr++;
			break;
		}

		*bStr++ = *fStr++;
	}

	*bStr = '\0';

	while ( isspace( *fStr ) )
		fStr++;

	return fStr;
}

char *GMCPStrip( char *text )
{
	static char buf[MAX_PROTOCOL_BUFFER];
	int i, x = 0;

	buf[x] = '\0';

	for ( i = 0; text[i] != '\0'; i++ )
	{
		switch ( text[i] )
		{
			default:
				buf[x++] = text[i];
			break;

			case '"':
				buf[x++] = '\\';
				buf[x++] = '"';
			break;

			case '\\':
				buf[x++] = '\\';
				buf[x++] = '\\';
			break;
		}
	}

	buf[x] = '\0';

	return buf;
}
/*************** END GMCP ***************/
