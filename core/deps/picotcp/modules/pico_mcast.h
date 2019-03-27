#ifndef INCLUDE_PICO_MCAST
#define INCLUDE_PICO_MCAST

#define MCAST_MODE_IS_INCLUDE                 (1)
#define MCAST_MODE_IS_EXCLUDE                 (2)
#define MCAST_CHANGE_TO_INCLUDE_MODE          (3)
#define MCAST_CHANGE_TO_EXCLUDE_MODE          (4)
#define MCAST_ALLOW_NEW_SOURCES               (5)
#define MCAST_BLOCK_OLD_SOURCES               (6)
#define MCAST_EVENT_DELETE_GROUP              (0x0)
#define MCAST_EVENT_CREATE_GROUP              (0x1)
#define MCAST_EVENT_UPDATE_GROUP              (0x2)
#define MCAST_EVENT_QUERY_RECV                (0x3)
#define MCAST_EVENT_REPORT_RECV               (0x4)
#define MCAST_EVENT_TIMER_EXPIRED             (0x5)
#define MCAST_NO_REPORT                       (1)

PACKED_STRUCT_DEF mcast_parameters {
    uint8_t event;
    uint8_t state;
    uint8_t general_query;
    uint8_t filter_mode;
    uint8_t last_host;
    uint16_t max_resp_time;
    union pico_address mcast_link;
    union pico_address mcast_group;
    struct pico_tree *MCASTFilter;
    struct pico_frame *f;
};

PACKED_STRUCT_DEF pico_mcast_group {
    uint8_t filter_mode;
    uint16_t reference_count;
    union pico_address mcast_addr;
    struct pico_tree MCASTSources;
};

PACKED_STRUCT_DEF mcast_filter_parameters {
    struct mcast_parameters *p;
    struct pico_tree *allow;
    struct pico_tree *block;
    struct pico_tree *filter;
    uint16_t sources;
    uint8_t proto;
    uint8_t record_type;
    struct pico_mcast_group *g;
    union pico_link *link;
};


extern int8_t pico_mcast_generate_filter(struct mcast_filter_parameters *filter, struct mcast_parameters *p);

#endif
