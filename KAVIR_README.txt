/******************************************************************************
 Protocol snippet by KaVir.   Version 8: 28-Oct-2012 (changes at end of file).

 This work has been released into the public domain by the copyright holder.

 In case this is not legally possible:

 The copyright holder grants any entity the right to use this work for any 
 purpose, without any conditions, unless such conditions are required by law.
 ******************************************************************************/

There are no licence conditions or restrictions to worry about.  So feel free 
to copy whatever you need, or just use it as a reference if you prefer.

This was originally just supposed to be an MSDP snippet, but I got a little 
carried away and ended up adding several other features as well.  Thanks go to 
Donky for inspiring me to start playing with protocols, and to Scandum for 
creating the specifications for MSDP and MSSP (and also for introducing me to 
XTerm 256 colours).  I'd also like to thank Tijer and Squiggle for testing the 
snippet and providing me with feedback, and to Bryantos for porting the snippet
to a couple of other codebases, and pointing out a missing statement in the 
installation instructions.  Tyche drew my attention to the problem of broken 
packets and failure to properly respond to client-initiated negotiation.  The 
MSSP table was updated in response to feedback from Rarva.Riendf on MudBytes.  
Version 6 contains a minor bugfix and clarification for using MCCP based on 
feedback from Jindrak.  Version 8 addresses feedback from Davion.

I've included instructions for adding the snippet to Diku/Merc, as it was first 
tested on Tijer's GodWars mud, but the snippet itself is codebase-neutral and 
should be fairly easy to use for any mud written in C.  It should take around 
5-10 minutes to install in most muds, as long as you know where the hooks need 
to go (see the appropriate INSTALL file for details).

The goal was to produce something that's easy to add and easy to use.  It's not 
exactly optimised for performance, but it shouldn't cause any problems, and the 
REPORTABLE option makes MSDP fairly light on bandwidth.

In order to make the snippet as easy as possible to add, I made it greedy.  If 
you were using other protocols, they'll just stop working, as the snippet will 
eat all the negotiation sequences.  Fixing this is a pretty simple job though, 
you just need to call your code from within the snippet's negotiation functions.

/******************************************************************************
 So what does it do?
 ******************************************************************************/

The snippet offers the following features:

Out-of-band communication between mud and client: Clients can choose either 
MSDP or ATCP to transmit data to and from the mud.  This is done invisibly, 
allowing you to update energy bars, icons, maps, etc, without anything being 
displayed in the text window.  These are the same protocols I used for the 
MUSHclient and Mudlet GUIs described on my blog: http://godwars2.blogspot.com

Extended colours: Allows you to embed XTerm 256 colours in strings (including 
help files, room descriptions, etc).  Automatically downgrades to the best-fit 
ANSI colour for clients that don't support extended colours.

Client detection: Allows you to view the name (and version when available) of 
the clients used by each of your players, see which protocols they support, and 
track the current size of their screen.

Clickable links: Allows you to embed clickable MXP links in strings (including 
help files, room descriptions, etc).  Automatically strips links before sending 
them to clients that don't support MXP.

Unicode support: Allows you to embed unicode characters within regular ASCII 
strings, and provide an alternative ASCII sequence that will be automatically 
substituted for clients that don't support UTF-8.

MSSP: Provides information about your mud to crawlers.  This is currently only 
used by sites such as MudBytes and MudStats, but could in theory be used for 
automatically generating and updating entire mud lists.

Sound support: Provides an extremely simple mechanism for sending sounds via 
MSDP or ATCP, with an automatic fallback for clients that only support MSP.  
You will need to provide your own soundpack of course!

/******************************************************************************
 How do I add the snippet to my mud?
 ******************************************************************************/

1. Follow the installation instructions in the INSTALL.TXT file.

2. Edit MUD_NAME in protocol.h, replacing "Unknown MUD" with your mud name.

3. Make sure the first section of protocol.c uses the correct headers and 
   functions for your mud.

/******************************************************************************
 How do I add a new MSDP variable?
 ******************************************************************************/

In protocol.h, add your new variable to the variable_t enum.

In protocol.c, add your new variable to the VariableNameTable[].  This needs 
to be in the same order as the enum (you'll get a runtime warning if it isn't).

Add a call to either MSDPSetString() or MSDPSetNumber() whenever the variable 
might change.  If the variable could be changed in multiple places, you can 
just add the call to msdp_update() instead.

/******************************************************************************
 I need to send my MSDP variable immediately, not once per second!
 ******************************************************************************/

You can call MSDPUpdate() after setting your variables, and it will immediately 
send them all to the player.  If you only want to flush a single variable, you 
can use MSDPFlush() instead - however, as with MSDPUpdate(), it will only send 
variables that are reportable and have changed.

/******************************************************************************
 The MSDP_GOLD variable reports a negative number!
 ******************************************************************************/

The snippet uses ints, which are usually 32 bits.  If you're using 64 bit long 
long ints for certain things (such as gold), then you should either store them 
in MSDP string variables, or change the snippet to use long long ints.

Note that this won't effect your character, it's only the MSDP variable that 
overflows.  It will give inaccurate information on your GUI, but players won't 
actually lose any gold, so don't panic!

/******************************************************************************
 The snippet is revealing my vnums!  Those are supposed to be top secret!
 ******************************************************************************/

I'm not quite sure why people are so protective about revealing vnums, but many 
are.  If you don't want to reveal them then you could have MSDP_ROOM_VNUM only 
update for imms, or even remove it entirely.  However if you wish to support 
the automappers that many major clients offer, you really need to provide your 
players with some way to uniquely identify their current room, and vnums are 
the easiest way of doing it.

If you don't want people mapping certain areas (such as mazes), then just send 
a vnum of '0' when in those areas.  You'll need a flag for this if you don't 
have one already.

/******************************************************************************
 My mud previously supported MCCP, but it's stopped working!
 ******************************************************************************/

The snippet intercepts and extracts all negotiation requests.  You will need to 
edit protocol.h and uncomment USING_MCCP, then update the CompressStart() and 
CompressEnd() functions in protocol.c to call whatever functions you normally 
use for starting and ending compression.

If you don't yet support MCCP and wish to, there are snippets available.  Jobo 
has some on his website here: http://www.dystopiamud.dk/snippets.php

MCCP increases the memory and CPU usage of your mud, but also gives significant 
bandwidth savings.  The exact savings will vary, but you can generally expect 
your bandwidth to be reduced to about 20% of its previous amount.

Please note that MCCP1 is obsolete, and should not be used.  All references to 
MCCP throughout this snippet refer to MCCP2 (telnet option 86).

If you also support copyover/hotreboot, please read the next section as well.

/******************************************************************************
 All the protocol information vanishes after I do a copyover/hotreboot!
 ******************************************************************************/

Some muds use an exec() function to replace the current process with a new one, 
effectively rebooting the mud without shutting it down.  The problem with this 
is that the client can't detect it, and because clients need to protect against 
negotiation loops they may end up ignoring your attempts to renegotiate.

Therefore in order to store the data across reboots, you need to save it when 
you do the copyover, and then load it again afterwards.

The snippet offers CopyoverSet() and CopyoverGet() functions to make this a 
bit easier.  When writing descriptors to the temporary file in your copyover 
code, add an extra "%s" to each row and copy the string from CopyoverGet() into 
it.  Then when you load the file again after the copyover, pass the string back 
into CopyoverSet(), and it'll restore the settings.

Note that this won't save the client name and version.  It's recommend that you 
instead save these in the player file (this means you can grep through player 
files to collect client usage statistics, which can be quite useful).

If you do start saving things in the player file, once again be particularly 
careful about the malloc/str_dup free/free_string thing.  If you mix them you 
may end up with some nasty bugs that are hard to track down.  As I mentioned 
earlier, it's well worth going through the snippet and making sure it uses 
the same functions as the rest of your mud.

Note also that CopyoverSet() calls CompressStart(), while CopyoverGet() calls 
CompressEnd().  So if you're using both copyover and MCCP, you shouldn't need 
to manually switch compression off and back on when doing a copyover, it should 
be done for you automatically.

/******************************************************************************
 None of the official ATCP variables work!
 ******************************************************************************/

This snippet primarily uses MSDP for transmitting data.  Unfortunately not all 
clients support MSDP, so I offer an alternative - you can use ATCP instead, 
with the MSDP variables treated as if they were a custom ATCP package.

This is really just a workaround.  The muds that originally used ATCP are now 
migrating over to GMCP, so I'm just "borrowing" their old telnet option.  My 
workaround doesn't break the ATCP specification (because the MSDP variables all 
go into their own package), but neither does it implement the official options.

If you actually want full ATCP, I'd suggest implementing GMCP instead.

/******************************************************************************
 Why don't you support GMCP/ZMP/102/etc?
 ******************************************************************************/

Because the original intent of this snippet was to add MSDP support.  I got a 
bit carried away, and ended up adding various other features that I felt could 
improve the user interface, but the main focus of the snippet is still MSDP.

/******************************************************************************
 How do I view/modify information about the clients my players are using?
 ******************************************************************************/

All of the data is stored in the protocol structure, it can be read and changed 
just like any other data.  For example ScreenWidth and ScreenHeight are simple 
integers, while client name and version are MSDP strings.  You can view the 
full list of options in the protocol.h file.

An IMPORTANT word of warning though:

Although I've added a small section at the top of protocol.c to make it easier 
to integrate into Diku/Merc derivatives, the snippet is designed to be codebase 
independent.  It has its own functions and its own types, and in particular it 
uses the standard C malloc() and free() functions for strings.

If you use this in a Diku you're probably using str_dup() and free_string() for 
strings.  Well whatever you do, do NOT mix and match.  If you allocate memory 
with str_dup() you MUST free it with free_string() - and if you allocate it 
with malloc(), you MUST free it with free().

In fact you should probably go through the snippet and change it to use the 
same functions as the rest of your mud for allocating and freeing memory.  
Update the types as well, and use your own equivalent of MatchString(), etc.

/******************************************************************************
 How do I use sound?
 ******************************************************************************/

Place a call to SoundSend() in your code.  You should do this before sending 
the associated message, because when sending an MSP trigger there is no newline 
(you could in theory add one to the SoundSend() function, but then the user 
would see a blank line every time they received a sound).

The client can enable sound through ATCP/MSDP by setting SOUND to 1.  If the 
client supports ATCP or MSDP, then these will be used to send out-of-band sound 
triggers to the client, which can then be played using a plugin or script.

For other clients, you will need to provide a command for switching sound on 
and off.  If the bSound variable in the protocol structure is set to true, and 
the client doesn't support ATCP or MSDP, then the snippet will send an old 
MSP-style in-band sound trigger.

If a client is using MSP sound triggers, then any text sequences of "!!SOUND(" 
sent to them will be instead be displayed as "!?SOUND(", so that players can't 
trigger sounds through chats, tells, etc.

/******************************************************************************
 How do I update the MSSP fields?
 ******************************************************************************/

Edit protocol.c and do a text search for "MSSPTable".  Remove the comments from 
around the variables you wish to use, and fill in the fields according the MSSP 
specification, which you can read here: http://tintin.sourceforge.net/mssp/

The "NAME" should already be defined if you've edited MUD_NAME in protocol.h, 
while PLAYERS and UPTIME will be calculated for you automatically.

/******************************************************************************
 How do I use extended colour?
 ******************************************************************************/

The special character used to indicate the start of a colour sequence is '\t' 
(i.e., a tab, or ASCII character 9).  This makes it easy to include in help 
files (as you can literally press the tab key) as well as strings (where you 
can use "\t" instead).  However players can't send tabs (on most muds at 
least), so this stops them from sending colour codes to each other.

The predefined colours are:

   n: no colour (switches colour off)

   r: dark red                          R: light red
   g: dark green                        G: light green
   b: dark blue                         B: light blue
   y: dark yellow                       Y: light yellow
   m: dark magenta                      M: light magenta
   c: dark cyan                         C: light cyan
   w: dark white                        W: light white

   a: dark azure                        A: light azure
   j: dark jade                         J: light jade
   l: dark lime                         L: light lime
   o: dark orange                       O: light orange
   p: dark pink                         P: light pink
   t: dark tan                          T: light tan
   v: dark violet                       V: light violet

So for example "This is \tOorange\tn." will colour the word "orange".  You can 
add more colours by updating the switch statement in ProtocolOutput(), and if 
you're using your own colour code, it can use extended colours in the same way.

It's also possible to explicitly specify an RGB value, by including a four 
character colour sequence within square brackets, eg:

   This is a \t[F010]very dark green foreground\tn.

Or:

   This is a \t[B210]dark brown background\tn.

The first character is either 'F' for foreground or 'B' for background.  The 
next three characters are the RGB (red/green/blue) values, each of which must 
be a digit in the range 0 (very dark) to 5 (very light).

Finally, it's also possible to retrieve the colour code directly by calling the 
ColourRGB() function.  This uses a static buffer, so make sure you copy the 
result after each call, don't do a sprintf() with multiple ColourRGB() calls.

Note that sending extended colours to a terminal that doesn't support them can 
have some very strange results.  The snippet therefore automatically downgrades 
to the best-fit ANSI colour for users that don't support extended colours.

Because there is no official way to detect support for extended colours, the 
snippet tries to work it out indirectly, erring on the side of caution.  If the 
b256Support variable in the protocol structure is set to "eSOMETIMES", that 
means some versions of this client are known to support extended colour - you 
will need to ask the user, and then set eMSDP_XTERM_256_COLORS to 1 (or they 
can do this themselves through MSDP/ATCP).

/******************************************************************************
 My players can't send tab characters, how can they use colour?
 ******************************************************************************/

If you wish to let your players use colour codes, uncomment COLOUR_CHAR in 
protocol.h.  This will allow people to use ^r for dark red, ^B for light blue, 
and so on, using the same predefined colours as described above.  You can also 
change COLOUR_CHAR to a different character if you prefer.

Colours can be disabled with "\t-" and enabled with "\t+", so (for example) if 
you don't want players to add colour to their chats, add "\t-" to the front of 
their message.  This needs to be done each time a string is sent.  If you want 
colours disabled by default, set COLOUR_ON_BY_DEFAULT (in protocol.h) to false.

Should you wish to let players use the extended colour codes as well, uncomment 
EXTENDED_COLOUR.  This will allow people to send codes like ^[F135] or ^[B332].

You may also uncomment DISPLAY_INVALID_COLOUR_CODES if you wish invalid codes 
to be displayed rather than hidden.

/******************************************************************************
 How do I use unicode characters?
 ******************************************************************************/

Unicode characters can be displayed in a similar way to colour, using square 
brackets to provide both a unicode value and an ASCII substitute.  For example:

   \t[U9814/Rook]

The above will draw a rook (the chess piece - unicode value 9814) if the client 
supports UTF-8, otherwise it'll display the text "Rook".

As with extended colour, support for UTF-8 is detected automatically - in this 
case using the CHARSET telnet option.  However it's not possible to detect if 
their font includes that particular character, or even if they're actually 
using a unicode font at all, so some care will need to be taken.

A free unicode font that I've found good is Fixedsys Excelsior, which you can 
download from here: http://www.fixedsysexcelsior.com/

Also of interest: http://en.wikipedia.org/wiki/List_of_Unicode_characters

/******************************************************************************
 How do I use clickable MXP links?
 ******************************************************************************/

You can add MXP tags in the same way as colour and unicode.  The easiest and 
safest way to do this is via the ( and ) bracket options.  For example:

   From here, you can walk \t(north\t).

This will turn the word "north" into a clickable link - you can click on it to 
execute the "north" command.

However it's also possible to include more explicit MXP tags, like this:

   The baker offers to sell you a \t<send href="buy pie">pie\t</send>.

As with the extended colour, MXP tags will be automatically removed if the user 
doesn't support MXP - but it's very important you remember to close the tags.

In theory you could also use other MXP options, such as graphics and sound, but 
be aware that MXP is implemented very inconsistently across clients - you'd be 
better off using MSDP instead.  If you do play with other MXP options, the 
pMXPVersion variable in the protocol structure will tell you which version of 
MXP the client is using, and you can use MXPSendTag() with the "<SUPPORT>" tag 
to find out exactly which options they support.  You can also embed another 
tag within strings to indicate which version of MXP is required:

   \t[X1.0]Special MXP data

In the above example, "\t[X1.0]" temporarily blocks MXP if the client is using 
a version of MXP below 1.0.  The block only applies to the next MXP tag, after 
that it is automatically cleared.

It's worth noting that MXP also supports 24-bit colour, which you may want to 
investigate.  Personally I've found that 256 colours are more than enough.

/******************************************************************************
 How do I use the Mudlet autoinstaller for my custom GUI?
 ******************************************************************************/

Uncomment MUDLET_PACKAGE at the top of protocol.h, and replace the URL with the 
one for your Mudlet package.  This should be a zip file containing one script 
(an XML file) and a folder containing whatever graphics and/or sound your GUI 
uses.  You may wish to rename the file from ".zip" to ".mpackage" to make it 
clear it's a Mudlet package, although this isn't strictly necessary.

If you update your GUI you will also need to update the version number so that 
Mudlet knows it needs to download a newer version.  The "1" before the package 
name represents the version number, so simply increment it by 1.

/******************************************************************************
 Your snippet thinks my client doesn't support X, but actually it does!
 ******************************************************************************/

I've covered what I could, but some of these things (particularly XTerm 256 
colour) are difficult to detect, so there will be clients that I've missed.

If you're a developer on a mud that uses this snippet, it should be fairly easy 
to add new clients to the list - but you should also add some in-game commands 
allowing players to manually switch the various options on and off.

If you're a client developer, you could add MSDP to your client and use the 
configurable variables to switch on XTerm 256 colour, UTF-8, etc.

/******************************************************************************
 My hosting service uses mudcheck.pl, and it keeps killing my mud!
 ******************************************************************************/

The mudcheck script checks the first line it receives after connecting, which 
is now a negotiation sequence.  You need to send a newline (or whatever the 
script is checking for) first.

Add another Write() to the ProtocolNegotiate() function:

void ProtocolNegotiate( descriptor_t *apDescriptor )
{
   static const char DoTTYPE [] = { (char)IAC, (char)DO, TELOPT_TTYPE, '\0' };
   Write(apDescriptor, "\n");    /* <--- Add this line */
   Write(apDescriptor, DoTTYPE);
}

/******************************************************************************
 My mud no longer hides passwords as they're being typed in, what happened?
 ******************************************************************************/

Version 7 of the snippet added support for client-initiated negotiation, and 
this included ECHO.  If you updated from an earlier version of the snippet, 
you'll need to switch to the new ProtocolNoEcho() function.  This is explained 
in detail in the installation instructions.

/******************************************************************************
 I already added an older version, what do I need to change to update?
 ******************************************************************************/

Ideally you should diff the old protocol.h and protocol.c against the new ones 
to see exactly what has changed.  At the very least, have a brief look to give 
yourself a general idea.  You should also make a backup of your old stuff.

At the top of your protocol.c is a section with mud-specific stuff, you will 
almost certainly have changed the #include, the Write() and the ReportBug(), 
so make sure you copy them across to the new protocol.c file.

In the MSSPTable[] in your old protocol.c file, copy everything between here:

   /* Generic */
   { "CRAWL DELAY",        "-1" },

And here:

   { NULL, NULL } /* This must always be last. */

Then paste it into the new protocol.c file.  Do the same in protocol.h:

   #define MUD_NAME "Unknown MUD"

   typedef struct descriptor_data descriptor_t;

Make sure the above is updated to be the same as your old protocol.h file.

Take a look in the appropriate INSTALL file and see how the MSDP_AFFECTS and 
MSDP_ROOM_EXITS now use tables.  It is recommended that you do the same, but 
don't forget to update your MUSHclient plugin and/or Mudlet script as well!

Some muds internally modify or strip out characters with ASCII values of 3, 4, 
5 and/or 6.  If your mud does this, you will need to change it before you can 
use the MSDP tables and arrays.

As the protocol_t structure has changed, it's essential that you do a clean 
make after updating to the latest version.

/******************************************************************************
 What's new in this version?
 ******************************************************************************/

As people use the snippet, occasional problems are revealed and reported, and I 
resolve them for the next version.  Furthermore, the specifications for some of 
the protocols (particularly MSDP) change over time, and the snippet needs to be 
updated to remain compliant.  The following summary describes each version:

Version 2 (13-Jun-2011)

* Added support for broken packets.

* Resolved a cyclic TTYPE issue that caused Windows telnet to freeze.

* Added the new MSSP variables.

Version 3 (28-Aug-2011)

* Added an AllocString() wrapper function, as strdup() isn't standard C.

* Added support for the new MSDP tables and arrays.

* Added support for the new UNREPORT and RESET MSDP commands.

* Added a new REPORTED_VARIABLES list, as described in the latest MSDP spec.

* Renamed VARIABLES to SENDABLE_VARIABLES as described in the latest MDSP spec.

* Cleaned up the code, adding consts and fixing -ansi and -pedantic warnings.

* Added support for the new Mudlet GUI autoinstaller.

* Added an MCCP flag to make integration with the snippet easier.

* Updated CopyoverGet() and CopyoverSet() to include TTYPE, MCCP and CHARSET.

* Added an MSDPFlush() function for variables that need to be sent immediately.

* ProtocolOutput() now lets you send tabs.

* The snippet now recognises that DecafMUD supports 256 colours.

* Updated the TBA instructions with a fix for strfrmt().

* Updated the installation instructions to use MSDP tables.

Version 4 (31-Aug-2011)

* Quick fix to AllocString().

Version 5 (12-Oct-2011)

* Added symbolic constants for MSDP_TABLE_OPEN/CLOSE and MSDP_ARRAY_OPEN/CLOSE.

* MSDPSetArray() was using table values rather than the array values.  Fixed.

* Added MSDPSendList(), used for the MSDP LIST command.

* Doubled MAX_VARIABLE_LENGTH for the list variables.

* Some of the LISTs had no separators between values when using ATCP.  Fixed.

* The MSSP table now uses function pointers, making it easier to update.

* Added support for both variants of MXP negotiation.

Version 6 (16-Nov-2011)

* Removed a stray semicolon at the end of an 'if' statement.

* Made it easier to add MCCP support.

* Made several minor updates to the installation instructions.

* Added an INSTALL_ROM.TXT.

Version 7 (22-Aug-2012)

* The mud now responds correctly to client-initiated negotiation.

* MSDP and ATCP must now be be negotiated before being used.

* Added support for more traditional-style mud colour codes.

* Extended the list of default colours.

* Clients with an autoconnect option now renegotiate if necessary.

* Added a function for switching ECHO on and off.

* Clarified the licensing conditions in the event that PD isn't possible.

Version 8 (28-Oct-2012)

* The Negotiated[] array wasn't being initialised.  Fixed.

* Added exp max/tnl to the ROM installation instructions.