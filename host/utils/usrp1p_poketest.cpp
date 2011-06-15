//FPGA register poke test for USRP1P
//uses UHD libusb transport

#include <uhd/device.hpp>
#include <uhd/transport/usb_zero_copy.hpp>
#include <uhd/transport/bounded_buffer.hpp>
#include <uhd/transport/usb_control.hpp>
#include <uhd/utils/assert.hpp>
#include <boost/shared_array.hpp>
#include <boost/foreach.hpp>
#include <boost/thread.hpp>
#include <boost/format.hpp>
#include <vector>
#include <iostream>
#include <iomanip>
#include "../lib/usrp/usrp1p/ctrl_packet.hpp"

//so the goal is to open a USB device to endpoints (2,6), submit a buffer, receive a reply, and compare them.
//use usb_zero_copy::make() to get a usb_zero_copy object and then start submitting.
//need to get a usb dev handle to pass to make
//use static std::vector<usb_device_handle::sptr> get_device_list(boost::uint16_t vid, boost::uint16_t pid) to get a device handle
//then get_send_buffer, send, etc.
using namespace uhd;
using namespace uhd::transport;

const boost::uint16_t ctrl_xfer_size = 32;

int main(int argc, char *argv[]) {
    std::cout << "USRP1+ GPIF poke test" << std::endl;
    //step 1: get a handle on it
    std::vector<usb_device_handle::sptr> handles = usb_device_handle::get_device_list(0xfffe, 0x0003);
    if(handles.size() == 0) {
        std::cout << "No USRP1+ found." << std::endl;
        return ~0;
    }
    
    bool verbose = false;
    if(argc > 1) if(std::string(argv[1]) == "-v") verbose = true;
    
    usb_device_handle::sptr handle = handles.front();

    usb_zero_copy::sptr data_transport;
    usb_control::sptr ctrl_transport = usb_control::make(handle); //just in case

    data_transport = usb_zero_copy::make(
                handle,        // identifier
                8,             // IN endpoint
                4,             // OUT endpoint
                uhd::device_addr_t("recv_frame_size=32, num_recv_frames=1, send_frame_size=32, num_send_frames=1") //args
    );
    
    if(verbose) std::cout << "Made." << std::endl;
    
    //ok now we're made. time to get a buffer and start sending data.

    managed_send_buffer::sptr sbuf;
    managed_recv_buffer::sptr rbuf;
    size_t xfercount = 0;
    
    static uint8_t sequence = 0;
    //uhd::usrp::ctrl_packet_out_t outpkt;
    //memset(outpkt.data, 0x00, sizeof(outpkt.data));
//    outpkt.op = uhd::usrp::CTRL_PACKET_WRITE;
//    outpkt.callbacks = 0;
//    outpkt.seq = sequence++;
//    outpkt.len = 4;
//    outpkt.addr = 0x00000000;
//    outpkt.data[0] = 0xff;
//    outpkt.data[1] = 0xfe;
//    outpkt.data[2] = 0xfd;
//    outpkt.data[3] = 0xfc;

    boost::uint16_t outpkt[16];
    /* Packet format:
     * Command: 2 bits
     * Callbacks: 6 bits
     * Seq num: 8 bits
     * Length: 16 bits
     * Addr LSW: 16 bits
     * Addr MSW: 16 bits
     * Data: 24 bytes/12 words
     * Lengths are in lines
     * 
     * readback:
     * AA00 LEN(16) SEQ(16) ADDR(32) DATA(16bx12B)
     */
    memset(outpkt, 0x00, sizeof(outpkt));
    outpkt[0] = 0x8000; //read cmd + callbacks (0) + seq
    outpkt[1] = 0x0001; //len
    outpkt[2] = 0x0000; //addr LSW
    outpkt[3] = 0x0000; //addr MSW
    outpkt[4] = 0x0A0A; //data
    outpkt[5] = 0xFFFF;
    
    
    srand(time(0));
//    while(1) {

        if(verbose) std::cout << "Getting send buffer." << std::endl;
        sbuf = data_transport->get_send_buff();
        if(sbuf == 0) {
            std::cout << "Failed to get a send buffer." << std::endl;
            return ~0;
        }
        
        for(int i = 0; i < ctrl_xfer_size; i++) {
            sbuf->cast<boost::uint8_t *>()[i] = ((boost::uint8_t *)&outpkt)[i];
        }
        
        if(verbose) std::cout << "Buffer loaded" << std::endl;

        sbuf->commit(ctrl_xfer_size);
        if(verbose) std::cout << "Committed." << std::endl;

        rbuf = data_transport->get_recv_buff(0.3); //timeout
        
        if(rbuf == 0) {
            std::cout << "Failed to get receive buffer (timeout?)" << std::endl;
            return ~0;
        }
        
        for(int j = 0; j < 32; j++) {
            std::cout << boost::format("%02X ") % int(rbuf->cast<const boost::uint8_t *>()[j]);
        }
        std::cout << std::endl;

        sbuf.reset();
        rbuf.reset();
        xfercount++;
        //if(verbose) std::cout << "sptrs reset" << std::endl;
//    }
    
    return 0;
}
