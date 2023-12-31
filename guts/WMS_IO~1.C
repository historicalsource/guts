/*
 *
 * $Id: wms_ioasic.c,v 1.2 1997/07/01 20:40:44 shepperd Exp $
 *
 * Name:        wms_ioasic.c
 *
 * Description: This file contains the unlock code for WMS's IOASIC.
 *
 * Modification History:
 *   Date       Who     Description
 * 04/07/97	dms	Hacked out the IOASIC unlock code from WMS's uartfunc.c
 *			into this file converting it to GUTS format in the process.
 */

#include <config.h>

#define SWAP(x) x
#define SWAP2(x) x
#define PIC_READY       0x0001      /*  Status      */
#define ASIC_WORD       ((U16 *) IOASIC_BASE)
#define MAIN_STATUS     ((U16 *) IO_MAIN_STS)
#define MAIN_CONTROL    ((U16 *) IO_MAIN_CTL)

struct IOErrorStruct {
    U32 ASICFirst ;       /* First portion of UnLock - PIC !Ready ()      */
    U32 ASICSecond ;      /* Couldn't write Host Arm Pattern              */
    U32 ASICThird ;       /* Couldn't write 16k CLL Data                  */
    U32 ASICFourth ;      /* Couldn't write Host Unlock Pattern           */
    U32 ASICFifth ;       /* Status bits didn't settle to zero at end     */
} ;
static struct IOErrorStruct IOErrors [3] ;

/*
 * This array contains addresses of where to write the various portions of
 * stuff to the I/O ASIC in order to accomplish the UnLock proccess.
 * NOTE: VERRRY DEPENDENT ON misc.h
 */

static U32 * const PICAddr [] = {
     (U32 *) (IOASIC_BASE + (3*BUS_SCALE)),
     (U32 *) (IOASIC_BASE + (2*BUS_SCALE)),    
     (U32 *) (IOASIC_BASE + (0*BUS_SCALE))
} ;

/*
 * These two arrays simply contain values and addresses to write the
 * values to in order to unlock the I/O ASIC.
 */

static const U32 PIC1 [] = {
    SWAP2(0x002b),
    SWAP2(0x0093),
    SWAP2(0x00a7),
    SWAP2(0x004e),
    SWAP2(0x0000),
    SWAP2(0x000e),
    -1
} ;

static const U32 PIC2 [] = {
     SWAP2(0x6f43), SWAP2(0x7970), SWAP2(0x6972), SWAP2(0x6867), SWAP2(0x2074), SWAP2(0x4328), SWAP2(0x2029), SWAP2(0x3931),
     SWAP2(0x3539), SWAP2(0x4d20), SWAP2(0x6469), SWAP2(0x6177), SWAP2(0x2079), SWAP2(0x614d), SWAP2(0x756e), SWAP2(0x6166),
     SWAP2(0x7463), SWAP2(0x7275), SWAP2(0x6e69), SWAP2(0x2067), SWAP2(0x6f43), SWAP2(0x706d), SWAP2(0x6e61), SWAP2(0x2079),
     SWAP2(0x2d2d), SWAP2(0x5020), SWAP2(0x6f72), SWAP2(0x7270), SWAP2(0x6569), SWAP2(0x6174), SWAP2(0x7972), SWAP2(0x2d20),
     SWAP2(0x202d), SWAP2(0x7375), SWAP2(0x2065), SWAP2(0x7570), SWAP2(0x7372), SWAP2(0x6175), SWAP2(0x746e), SWAP2(0x7420),
     SWAP2(0x206f), SWAP2(0x694d), SWAP2(0x7764), SWAP2(0x7961), SWAP2(0x4d20), SWAP2(0x6e61), SWAP2(0x6675), SWAP2(0x6361),
     SWAP2(0x7574), SWAP2(0x6972), SWAP2(0x676e), SWAP2(0x4320), SWAP2(0x6d6f), SWAP2(0x6170), SWAP2(0x796e), SWAP2(0x6920),
     SWAP2(0x736e), SWAP2(0x7274), SWAP2(0x6375), SWAP2(0x6974), SWAP2(0x6e6f), SWAP2(0x2073), SWAP2(0x2d2d), SWAP2(0x6620),
     SWAP2(0x726f), SWAP2(0x6920), SWAP2(0x746e), SWAP2(0x7265), SWAP2(0x616e), SWAP2(0x206c), SWAP2(0x7375), SWAP2(0x2e65),
     SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020),
     SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020),
     SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020),
     0
};

static const U32 PIC3 [] = {
     SWAP2(0x4343), SWAP2(0x6f6f), SWAP2(0x7070), SWAP2(0x7979), SWAP2(0x7272), SWAP2(0x6969), SWAP2(0x6767), SWAP2(0x6868),
     SWAP2(0x7474), SWAP2(0x2020), SWAP2(0x2828), SWAP2(0x4343), SWAP2(0x2929), SWAP2(0x2020), SWAP2(0x3131), SWAP2(0x3939),
     SWAP2(0x3939), SWAP2(0x3535), SWAP2(0x2020), SWAP2(0x4d4d), SWAP2(0x6969), SWAP2(0x6464), SWAP2(0x7777), SWAP2(0x6161),
     SWAP2(0x7979), SWAP2(0x2020), SWAP2(0x4d4d), SWAP2(0x6161), SWAP2(0x6e6e), SWAP2(0x7575), SWAP2(0x6666), SWAP2(0x6161),
     SWAP2(0x6363), SWAP2(0x7474), SWAP2(0x7575), SWAP2(0x7272), SWAP2(0x6969), SWAP2(0x6e6e), SWAP2(0x6767), SWAP2(0x2020),
     SWAP2(0x4343), SWAP2(0x6f6f), SWAP2(0x6d6d), SWAP2(0x7070), SWAP2(0x6161), SWAP2(0x6e6e), SWAP2(0x7979), SWAP2(0x2020),
     SWAP2(0x2d2d), SWAP2(0x2d2d), SWAP2(0x2020), SWAP2(0x5050), SWAP2(0x7272), SWAP2(0x6f6f), SWAP2(0x7070), SWAP2(0x7272),
     SWAP2(0x6969), SWAP2(0x6565), SWAP2(0x7474), SWAP2(0x6161), SWAP2(0x7272), SWAP2(0x7979), SWAP2(0x2020), SWAP2(0x2d2d),
     SWAP2(0x2d2d), SWAP2(0x2020), SWAP2(0x7575), SWAP2(0x7373), SWAP2(0x6565), SWAP2(0x2020), SWAP2(0x7070), SWAP2(0x7575),
     SWAP2(0x7272), SWAP2(0x7373), SWAP2(0x7575), SWAP2(0x6161), SWAP2(0x6e6e), SWAP2(0x7474), SWAP2(0x2020), SWAP2(0x7474),
     SWAP2(0x6f6f), SWAP2(0x2020), SWAP2(0x4d4d), SWAP2(0x6969), SWAP2(0x6464), SWAP2(0x7777), SWAP2(0x6161), SWAP2(0x7979),
     SWAP2(0x2020), SWAP2(0x4d4d), SWAP2(0x6161), SWAP2(0x6e6e), SWAP2(0x7575), SWAP2(0x6666), SWAP2(0x6161), SWAP2(0x6363),
     SWAP2(0x7474), SWAP2(0x7575), SWAP2(0x7272), SWAP2(0x6969), SWAP2(0x6e6e), SWAP2(0x6767), SWAP2(0x2020), SWAP2(0x4343),
     SWAP2(0x6f6f), SWAP2(0x6d6d), SWAP2(0x7070), SWAP2(0x6161), SWAP2(0x6e6e), SWAP2(0x7979), SWAP2(0x2020), SWAP2(0x6969),
     SWAP2(0x6e6e), SWAP2(0x7373), SWAP2(0x7474), SWAP2(0x7272), SWAP2(0x7575), SWAP2(0x6363), SWAP2(0x7474), SWAP2(0x6969),
     SWAP2(0x6f6f), SWAP2(0x6e6e), SWAP2(0x7373), SWAP2(0x2020), SWAP2(0x2d2d), SWAP2(0x2d2d), SWAP2(0x2020), SWAP2(0x6666),
     SWAP2(0x6f6f), SWAP2(0x7272), SWAP2(0x2020), SWAP2(0x6969), SWAP2(0x6e6e), SWAP2(0x7474), SWAP2(0x6565), SWAP2(0x7272),
     SWAP2(0x6e6e), SWAP2(0x6161), SWAP2(0x6c6c), SWAP2(0x2020), SWAP2(0x7575), SWAP2(0x7373), SWAP2(0x6565), SWAP2(0x2e2e),
     SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020),
     SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020),
     SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020),
     SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020),
     SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020),
     SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020), SWAP2(0x2020),
     0
};

static const U32 PIC4 [] = {
    SWAP2(0x0054),
    SWAP2(0x0029),
    SWAP2(0x00e2),
    -1
} ;

/*
 * The number of times PICWrite attempts to write before giving up and
 * shitcanning the operation.
 */

#define PIC_MAX_WRITES 100

/*
 * These reside in GlobRAM.c used to signify whether the ASIC has been
 * unlocked, and the UART has been Initialized (Wow ! Cool names).
 */

static U32 ASICUnlocked ;

/*
 * Name:        PICReady
 * Synopsis:    S32 PICReady (void) ;
 * Description: This routine answers the question, "Can I talk with the PIC ?"
 * Inputs:      None.
 * Outputs:     FALSE means no, no PIC conversations at this time, else go
 *              right ahead and start talking.
 * Notes:       There was a version of this with complete resetting of the
 *              PIC, but this is the short one. 
 */


static S32 PICReady (void)
{
    U16 wValue, *wAddr ;

    wAddr = ASIC_WORD ;
    wValue = *wAddr ;
    return (wValue & PIC_READY) ;
}

/*
 * Name:        PICWrite
 * Synopsis:    S32 PICWrite (U32 Data, U32 *Address) ;
 * Description: This routine is called in order to attempt to write data
 *              to the PIC.
 * Inputs:      Data is the value that you want to write out, and Address
 *              is where you want to write it.
 * Outputs:     If the write was successfull, then TRUE is returned, else
 *              FALSE is the result of your folly.
 * Notes:       This routine will attempt to write up to PIC_MAX_WRITES
 *              before giving up and returning FALSE.
 */


static S32 PICWrite (U32 Data, U32 *Address)
{

    S32 i ;
    U16 *wAddr, wData ;

    wAddr = ASIC_WORD ;

/*
 * Attempt to write data up to PIC_MAX_WRITES times
 */

    for (i = 0; i < PIC_MAX_WRITES; i++) {
        wData = *wAddr ;
        if ((wData & 0xe000) != 0x2000)
            continue ;
        *Address = Data ;
        wData = *wAddr ;        /* read added here for debug */
        return (TRUE) ;
    }

/*
 * If we're here, the PIC's busy bit didn't come undone. In this case, return
 * 0 to the caller.
 */

    return (FALSE) ;
}

/*
 * Name:        UnLock
 * Synopsis:    S32 Unlock (void) ;
 * Description: This routine is called in order to unlock the I/O ASIC.
 * Inputs:      None.
 * Outputs:     If everything unlocked correctly, then the value 0 can be
 *              expected.  Otherwise, an error code (with meaning only to
 *              the programmer trying to unlock the sucker) is returned.
 * Notes:       There are some very ugly constants and such shit here.
 */

S32 UnLock (void)
{
    S32 i;
    U32 iValue, *iAddr;
    const U32 *iData;
    U16 wValue ;

#if RESET_CTL
    *(int *)RESET_CTL &= ~(1<<B_RESET_IOASIC);
    for (i = 0; i < 20000; i++)
      ;                    /* reset delay loop */
    *(int *)RESET_CTL |= (1<<B_RESET_IOASIC);
#else
    *(int *)IO_RESET = 0x00000000 ;
    for (i = 0; i < 20000; i++)
      ;                    /* reset delay loop */
    *(int *)IO_RESET = -1;
#endif

    for (i = 0; i < 5000000; i++) {
      if (PICReady ())
        break;
    }

    if (!PICReady ()) {
        IOErrors [0].ASICFirst++ ;
        return (1) ;
    }

    iAddr = PICAddr [0] ;

    for (i = 0;;) {
        if ((iValue = PIC1 [i++]) == (U32) -1)
            break ;
        if (PICWrite (iValue, iAddr))
            continue ;
        IOErrors [0].ASICSecond++ ;
        return (1) ;
    }

    iAddr = PICAddr [1] ;

    iData = PIC2 ;

    for (i = 0; i < 16384; i++) {
        if (!(iValue = *iData++)) {
            iData = PIC3 ;
            iValue = *iData++ ;
        }
        if (PICWrite (iValue, iAddr))
            continue ;
        IOErrors [0].ASICThird++ ;
        return (2) ;
    }            

    iAddr = PICAddr [2] ;

    for (i = 0;;) {
        if ((iValue = PIC4 [i++]) == (U32) -1)
            break ;
        if (PICWrite (iValue, iAddr))
            continue ;
        IOErrors [0].ASICFourth++ ;
        return (3) ;
    }

    for (i = 0; i < 20000; i++) {
        wValue = *MAIN_CONTROL ;
        if (wValue == 0)
            break ;
    }
    if (wValue) {
        IOErrors [0].ASICFifth++ ;
        return (1) ;
    }

    *MAIN_CONTROL = 0 ;
    
    ASICUnlocked = TRUE ;
    return (FALSE) ;
}
