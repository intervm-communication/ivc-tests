/**
 * Simple, hackish test backend for the userspace granting code test.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/mman.h>
#include <ctype.h>

extern "C" {
#include <xentoollog.h>
#include <xengnttab.h>
#include <xenstore.h>
}

#define PAGE_SIZE 4096

#define OTHERSIDE_STAGE (1)
#define OURSIDE_STAGE   (0)

/* Global referenece to our grant table interface. */
xengnttab_handle *gnttab;

/* Global reference to our logger. */
xentoollog_logger_stdiostream *logger;

/* Global reference to the xenstore. */
struct xs_handle *xs;

/* Global reference to the shared buffer. */
char * mapping;
char * data_buffer;

/**
 * Hackish (test-only) method that attempts to read a grant reference
 * from the test xenstore key. Returns 0xFFFFFFFF on failure.
 */
uint32_t get_shared_gref(int timeout)
{
    unsigned int len = 0, gref;
    void *value;

    /* Try to read the gref until we get a value,
     * or until the timeout passes. */
    while(1) {
        value = xs_read(xs, 0, "/test/gref", &len);

        if(value && len)
          break;

        if(value && !len)
          free(value);

        if(timeout <= 0)
          return 0xFFFFFFFF;

        timeout--;
        sleep(1);
    }

    /* Convert the XenStore value to an integer. */
    gref = atoi((char *)value);
    free(value);

    /* And return our gref. */
    return (uint32_t)gref;
}

/**
 * Hackish (test-only) method to wait for the other side to signal.
 * Assumes that integer additions are atomic on the given platform.
 */
int wait_for_other_side(int timeout)
{
    static char current_value = 0;

    while(mapping[OTHERSIDE_STAGE] == current_value) {
        sleep(1);

        if(timeout <= 0)
          return 0;

        timeout--;
    }

    current_value++;
    return 1;
}

/**
 * Hackish (test-only) method to signal to the other side.
 * Assumes that integer additions are atomic on the given platform.
 */
void signal_to_other_side()
{
    mapping[OURSIDE_STAGE]++;
}


int main(int argc, char **argv)
{
    uint32_t domid;
    uint32_t gref;
    int position;


    /* Ensure we have our arguments. */
    if(argc != 2) {
        printf("usage: %s [otherside_domid]\n\n", argv[0]);
        exit(0);
    }

    /* Fetch the domid to be read. */
    domid = atoi(argv[1]);

    /* Open our XenStore connection. */
    xs = xs_daemon_open();
    if(!xs) {
        printf("Failed to talk to the XenStore!\n");
        return -1;
    }

    /* Fetch the grant reference to be used. */
    gref = get_shared_gref(10);
    if(gref == 0xFFFFFFFF) {
        printf("Timed out waiting for gref.\n");
        return -1;
    }

    /* Bring up our main logger... */
    logger = xtl_createlogger_stdiostream(stdin, XTL_ERROR, XTL_STDIOSTREAM_SHOW_DATE);

    /* Bring up the granting interface. */
    gnttab = xengnttab_open((xentoollog_logger *)logger, 0);
    if(!gnttab) {
        printf("Failed to open gnttab!\n");
        exit(-1);
    }

    /* Grant out a page, and then wait. */
    mapping = (char *)xengnttab_map_grant_refs(gnttab, 1, &domid, &gref, PROT_READ | PROT_WRITE);
    data_buffer = mapping + 2;

    if(!mapping) {
        printf("Something's not right, bailing.\n");
        exit(-1);
    }

    // Test 1: Send a signal to the other side.
    strcpy(data_buffer, "Hello from dom0!");
    signal_to_other_side();

    // Test 2: Receive from the other side, and respond by
    // inverting the string.
    wait_for_other_side(10);
    for(char * c = data_buffer; *c; ++c)
        *c = toupper(*c);
    signal_to_other_side();

    /* Bring down the granting interface. */
    xengnttab_close(gnttab);
}
