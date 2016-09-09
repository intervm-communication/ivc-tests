#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>

extern "C" {
#include <xentoollog.h>
#include <xengnttab.h>
#include <xenstore.h>
}

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

#define PAGE_SIZE 4096

#define TEST_HOST_DOMID (0)

#define SIGNALING_TIMEOUT (10)
#define OTHERSIDE_STAGE (0)
#define OURSIDE_STAGE   (1)

/* Global referenece to our grant table interface. */
xengntshr_handle *gntshr;

/* Global reference to our logger. */
xentoollog_logger_stdiostream *logger;

/* Global reference to the xenstore. */
struct xs_handle *xs;

/* Global reference to our granted buffer. */
char * mapping = nullptr;
char * data_buffer;

uint32_t grant_out_page(int domid, char ** mapping)
{
    uint32_t granted_page[1];
    char * result = (char *)xengntshr_share_pages(gntshr, domid, 1, granted_page, 1);

    if(!mapping || !result)
      return -1;

    *mapping = result;
    return granted_page[0];
}

/**
 * Place the grant reference to our shared page into the XenStore for
 * consumption by the other side.
 */ 
void set_xenstore_grant(uint32_t gref)
{
    xs_transaction_t transaction;
    char gref_string[16];
    int len, rc;

    /* Create a string representation of our grant reference... */
    len = snprintf(gref_string, sizeof(gref_string), "%" PRIu32, gref);

    /* ... and share it with the other side via the xenstore. */
    transaction = xs_transaction_start(xs);
    xs_write(xs, transaction, "/test/gref", gref_string, len);
    xs_transaction_end(xs, transaction, false);
}


int main(int argc, char **argv)
{
    uint32_t gref;

    /* Open our XenStore connection. */
    xs = xs_daemon_open();
    if(!xs) {
        printf("Failed to talk to the XenStore!");
    }

    /* Bring up our main logger... */
    logger = xtl_createlogger_stdiostream(stdin, XTL_ERROR, XTL_STDIOSTREAM_SHOW_DATE);

    /* Bring up the granting interface. */
    gntshr = xengntshr_open((xentoollog_logger *)logger, 0);
    if(!gntshr) {
        printf("Failed to open gntshr!\n");
        int result = Catch::Session().run( argc, argv );
        exit(-1);
    }

    /* Grant out a page to the other side... */
    gref = grant_out_page(TEST_HOST_DOMID, &mapping);
    data_buffer = mapping + 2;

    /* ... and share that page's grant reference. */
    set_xenstore_grant(gref);

    /* Run the core unit tests. */
    int result = Catch::Session().run( argc, argv );

    /* Bring down the granting interface. */
    xengntshr_close(gntshr);

    return result;
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


SCENARIO("pages can be granted out from the userland", "[single_page]") {
    WHEN("a page is granted out") {
        THEN("a writeable buffer is alocated") {
            REQUIRE(mapping != nullptr);
            REQUIRE((*mapping = 0) == 0);
        }
    }
    WHEN("connected to dom0") {
        THEN("dom0 can communicate data to us") {
            REQUIRE(wait_for_other_side(SIGNALING_TIMEOUT));
            REQUIRE(!strcmp(data_buffer, "Hello from dom0!"));
        }
        THEN("we can communicate with dom0") {

            // Send a message to the domU...
            strcpy(data_buffer, "Hello from the domU!");
            signal_to_other_side();

            // ... and check for the response.
            REQUIRE(wait_for_other_side(SIGNALING_TIMEOUT));
            REQUIRE(!strcmp(data_buffer, "HELLO FROM THE DOMU!"));
        }
    }
}
