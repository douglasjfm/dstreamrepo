#include <gst/gst.h>
#include <stdlib.h>
#include <string.h>
/*

	@author Douglas Medeiros [<douglasjfm at gmail dot com>]

*/
static void make_request_pad_and_link (GstElement *makePad,const gchar *pad_template, GstElement *linkToBefore, GstElement *linkToAfter)
{
    GstPad * pad;
    gchar *name;
    gboolean ret;

    pad = gst_element_get_request_pad (makePad, pad_template);
    name = gst_pad_get_name (pad);
    //g_print ("A new pad %s was created\n", name);
    /* here, you would link the pad */
    if (linkToBefore)
    {
        GstPad *stcpad = gst_element_get_static_pad(linkToBefore,"src");
        ret = gst_pad_link(stcpad,pad);
        //    gst_object_unref(stcpad);
    }
    if (linkToAfter)
    {
        GstPad *stcpad = gst_element_get_static_pad(linkToAfter,"sink");
        ret = gst_pad_link(pad,stcpad);
        //   gst_object_unref(stcpad);
    }

    if (ret)
        g_print ("  Pad req '%s' but link failed.\n", name);
    else
        g_print ("  Pad req Link succeeded (type '%s').\n", name);

    g_free (name);
    /* and, after doing that, free our reference */
    //gst_object_unref (GST_OBJECT (pad));
}

static void pad_added_handler(GstElement *src, GstPad *new_pad, GstElement **sink)
{
    GstPad *link = NULL;
    GstPadLinkReturn ret;
    GstCaps *new_pad_caps = NULL;
    GstStructure *new_pad_struct = NULL;
    gchar *name_pad;
    const gchar *new_pad_type = NULL;


    g_print ("->handler: Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

    //g_print ("->Received new pad '%s' from '%s':\n", GST_PAD_NAME (new_pad), GST_ELEMENT_NAME (src));

    /* If our converter is already linked, we have nothing to do here */
//    if (gst_pad_is_linked(sink_pad))
//    {
//        g_print("  We are already linked. Ignoring.\n");
//        goto exit;
//    }

//	/* Check the new pad's type */
    if (g_str_has_prefix(name_pad,"send_rtp_src") || g_str_has_prefix(name_pad,"send_rtcp_sink"))
    {
        new_pad_caps = gst_pad_get_caps(new_pad);
        new_pad_struct = gst_caps_get_structure(new_pad_caps, 0);
        new_pad_type = gst_structure_get_name(new_pad_struct);
        name_pad = gst_pad_get_name(new_pad);
    }
    else return;

    /* Attempt the link */

    if (g_str_has_prefix(name_pad,"send_rtp_src"))
    {
        g_print("->->send rtp src detect\n");
        link = gst_element_get_pad(sink[0],"sink");

        if (gst_pad_is_linked(link))
        {
            gst_object_unref(link);
            goto exit;
        }
        ret = gst_pad_link(new_pad, link);
    }
    else if (g_str_has_prefix(name_pad,"send_rtcp_sink"))
    {
        g_print("->->send rtcp sink detect\n");
        link = gst_element_get_pad(sink[2],"src");

        if (gst_pad_is_linked(link))
        {
            gst_object_unref(link);
            goto exit;
        }
        ret = gst_pad_link(link, new_pad);
    }

    else goto exit;

    if (ret!=888 && GST_PAD_LINK_FAILED (ret))
    {
        g_print ("->->->Type is '%s' but link failed.\n", new_pad_type);
    }
    else
    {
        g_print ("->->->Link succeeded (type '%s').\n", new_pad_type);
    }

exit:
    /* Unreference the new pad's caps, if we got them */
    if (new_pad_caps != NULL)
        gst_caps_unref(new_pad_caps);
    if(link) gst_object_unref(link);
    /* Unreference the sink pad */
}

int main(int argc, char *argv[])
{
    GstElement *pipeline_v, *source_v, *sink_v, *enc_v, *conv_v;
    GstElement *pipeline_a, *source_a, *sink_a, *sinkrtcp, *sink3_a, *enc_a, *conv_a;
    char destIP[20];
    GMainLoop *app_loop;

    GstElement *sinkList[3];
    //GstElement *audioresamp;
    GstElement *rtpspxpay, *rtpbin;
    GstBus *bus;

    GstMessage *msg;
    GstStateChangeReturn ret;

    gchar *ipdest;
    char *mode = argv[1];

    if (argc < 2) ipdest = "192.168.1.20";
    else ipdest = (gchar*) argv[2];

    //GMain Loop
    app_loop = g_main_loop_new (NULL, FALSE);

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Create the elements */
    source_v = gst_element_factory_make("v4l2src",NULL);
    source_a = gst_element_factory_make("alsasrc",NULL);

    conv_a = gst_element_factory_make("audioconvert",NULL);
    conv_v = gst_element_factory_make("ffmpegcolorspace",NULL);

    enc_a = gst_element_factory_make("speexenc",NULL);
    enc_v = gst_element_factory_make("theoraenc",NULL);

    sink_a = gst_element_factory_make("udpsink",NULL);
    sinkrtcp = gst_element_factory_make("udpsink",NULL);
    sink_v = gst_element_factory_make("udpsink",NULL);
    sink3_a = gst_element_factory_make("udpsrc",NULL);

    rtpspxpay = gst_element_factory_make("rtpspeexpay",NULL);
    rtpbin = gst_element_factory_make("gstrtpbin",NULL);

    //audioresamp = gst_element_factory_make = ("audioresample",NULL);


    g_object_set(sink_a,"host",ipdest, "port", 12345, NULL);

    g_object_set(sink3_a, "port", 12347, NULL);
    g_object_set(sinkrtcp,"host",destIP, "port", 12346,"async",(gboolean) FALSE,"sync",(gboolean)FALSE, NULL);
    g_object_set(sink_v,"host",destIP,"port",12348,NULL);
    g_object_set(enc_v,"bitrate",400,NULL);

    /*Insert sinks in the GList*/
    sinkList[0] = sink_a;
    sinkList[1] = sinkrtcp;
    sinkList[2] = sink3_a;

    /* Create the empty pipeline */
    pipeline_a = gst_pipeline_new("audio-pipeline");
    pipeline_v = gst_pipeline_new("video-pipeline");

    if (!pipeline_a || !source_a || !sink_a || !enc_a || !conv_a || !pipeline_v || !source_v || !sink_v || !enc_v || !conv_v)
    {
        g_printerr("Not all elements could be created.");
        system("pause");
        return -1;
    }

    /* Build the pipeline */
    gst_bin_add(GST_BIN(pipeline_a), rtpbin);
    gst_bin_add(GST_BIN(pipeline_a), source_a);
    gst_bin_add(GST_BIN(pipeline_a), conv_a);
    gst_bin_add(GST_BIN(pipeline_a), enc_a);
    gst_bin_add(GST_BIN(pipeline_a), rtpspxpay);
    gst_bin_add(GST_BIN(pipeline_a), sink_a);
    gst_bin_add(GST_BIN(pipeline_a), sinkrtcp);
    gst_bin_add(GST_BIN(pipeline_a), sink3_a);

    gst_bin_add(GST_BIN(pipeline_v), source_v);
    gst_bin_add(GST_BIN(pipeline_v), conv_v);
    gst_bin_add(GST_BIN(pipeline_v), enc_v);
    gst_bin_add(GST_BIN(pipeline_v), sink_v);

    if (gst_element_link_many(source_a, conv_a, enc_a, rtpspxpay, NULL) != TRUE)
    {
        g_printerr("Elements audio could not be linked.\n");
        gst_object_unref(pipeline_a);
        gst_object_unref(pipeline_v);
        system("pause");
        return -1;
    }
    if (gst_element_link_many(source_v, conv_v, enc_v, sink_v, NULL) != TRUE)
    {
        g_printerr("Elements video could not be linked.\n");
        gst_object_unref(pipeline_v);
        gst_object_unref(pipeline_a);
        system("pause");
        return -1;
    }

    //g_signal_connect(demux, "pad-added", G_CALLBACK(pad_added_handler), dec);
    g_signal_connect(rtpbin, "pad-added", G_CALLBACK(pad_added_handler), sinkList);//tratar send_rtp_src tratar send_rtcp_src

    make_request_pad_and_link(rtpbin,"send_rtp_sink_%d",rtpspxpay,NULL);
    make_request_pad_and_link(rtpbin,"send_rtcp_src_%d",NULL,sinkrtcp);

    /* Start playing */
    //ret = gst_element_set_state(pipeline_a, GST_STATE_PLAYING);

    ret = gst_element_set_state(pipeline_v, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_printerr("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(pipeline_a);
        gst_object_unref(pipeline_v);

        system("pause");
        return -1;
    }

    g_print("playing");

    /* Wait until error or EOS */
    bus = gst_element_get_bus(pipeline_v);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));


    g_main_loop_run (app_loop);

    /* Parse message */

    gst_message_unref(msg);


    /* Free resources */
    gst_object_unref(bus);
    gst_element_set_state(pipeline_a, GST_STATE_NULL);
    gst_element_set_state(pipeline_v, GST_STATE_NULL);
    gst_object_unref(pipeline_a);
    gst_object_unref(pipeline_v);
    g_main_loop_unref(app_loop);
    return 0;
}
