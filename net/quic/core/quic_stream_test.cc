// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/quic_stream.h"

#include <memory>

#include "net/quic/core/quic_connection.h"
#include "net/quic/core/quic_utils.h"
#include "net/quic/core/quic_write_blocked_list.h"
#include "net/quic/core/spdy_utils.h"
#include "net/quic/platform/api/quic_flags.h"
#include "net/quic/platform/api/quic_logging.h"
#include "net/quic/platform/api/quic_ptr_util.h"
#include "net/quic/platform/api/quic_test.h"
#include "net/quic/platform/api/quic_test_mem_slice_vector.h"
#include "net/quic/test_tools/quic_config_peer.h"
#include "net/quic/test_tools/quic_connection_peer.h"
#include "net/quic/test_tools/quic_flow_controller_peer.h"
#include "net/quic/test_tools/quic_session_peer.h"
#include "net/quic/test_tools/quic_stream_peer.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/test/gtest_util.h"
#include "testing/gmock_mutant.h"

using std::string;
using testing::AnyNumber;
using testing::AtLeast;
using testing::CreateFunctor;
using testing::DoAll;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::StrictMock;
using testing::WithArgs;
using testing::_;

namespace net {
namespace test {
namespace {

const char kData1[] = "FooAndBar";
const char kData2[] = "EepAndBaz";
const size_t kDataLen = 9;
const bool kShouldProcessData = true;
const bool kShouldNotProcessData = false;

class TestStream : public QuicStream {
 public:
  TestStream(QuicStreamId id, QuicSession* session, bool should_process_data)
      : QuicStream(id, session) {}

  void OnDataAvailable() override {}

  MOCK_METHOD0(OnCanWriteNewData, void());

  using QuicStream::CanWriteNewData;
  using QuicStream::CloseWriteSide;
  using QuicStream::OnClose;
  using QuicStream::WriteMemSlices;
  using QuicStream::WriteOrBufferData;
  using QuicStream::WritevData;
  using QuicStream::fin_buffered;

 private:
  string data_;
};

class QuicStreamTest : public QuicTestWithParam<bool> {
 public:
  QuicStreamTest()
      : initial_flow_control_window_bytes_(kMaxPacketSize),
        zero_(QuicTime::Delta::Zero()),
        supported_versions_(AllSupportedTransportVersions()) {
    headers_[":host"] = "www.google.com";
    headers_[":path"] = "/index.hml";
    headers_[":scheme"] = "https";
    headers_["cookie"] =
        "__utma=208381060.1228362404.1372200928.1372200928.1372200928.1; "
        "__utmc=160408618; "
        "GX=DQAAAOEAAACWJYdewdE9rIrW6qw3PtVi2-d729qaa-74KqOsM1NVQblK4VhX"
        "hoALMsy6HOdDad2Sz0flUByv7etmo3mLMidGrBoljqO9hSVA40SLqpG_iuKKSHX"
        "RW3Np4bq0F0SDGDNsW0DSmTS9ufMRrlpARJDS7qAI6M3bghqJp4eABKZiRqebHT"
        "pMU-RXvTI5D5oCF1vYxYofH_l1Kviuiy3oQ1kS1enqWgbhJ2t61_SNdv-1XJIS0"
        "O3YeHLmVCs62O6zp89QwakfAWK9d3IDQvVSJzCQsvxvNIvaZFa567MawWlXg0Rh"
        "1zFMi5vzcns38-8_Sns; "
        "GA=v*2%2Fmem*57968640*47239936%2Fmem*57968640*47114716%2Fno-nm-"
        "yj*15%2Fno-cc-yj*5%2Fpc-ch*133685%2Fpc-s-cr*133947%2Fpc-s-t*1339"
        "47%2Fno-nm-yj*4%2Fno-cc-yj*1%2Fceft-as*1%2Fceft-nqas*0%2Fad-ra-c"
        "v_p%2Fad-nr-cv_p-f*1%2Fad-v-cv_p*859%2Fad-ns-cv_p-f*1%2Ffn-v-ad%"
        "2Fpc-t*250%2Fpc-cm*461%2Fpc-s-cr*722%2Fpc-s-t*722%2Fau_p*4"
        "SICAID=AJKiYcHdKgxum7KMXG0ei2t1-W4OD1uW-ecNsCqC0wDuAXiDGIcT_HA2o1"
        "3Rs1UKCuBAF9g8rWNOFbxt8PSNSHFuIhOo2t6bJAVpCsMU5Laa6lewuTMYI8MzdQP"
        "ARHKyW-koxuhMZHUnGBJAM1gJODe0cATO_KGoX4pbbFxxJ5IicRxOrWK_5rU3cdy6"
        "edlR9FsEdH6iujMcHkbE5l18ehJDwTWmBKBzVD87naobhMMrF6VvnDGxQVGp9Ir_b"
        "Rgj3RWUoPumQVCxtSOBdX0GlJOEcDTNCzQIm9BSfetog_eP_TfYubKudt5eMsXmN6"
        "QnyXHeGeK2UINUzJ-D30AFcpqYgH9_1BvYSpi7fc7_ydBU8TaD8ZRxvtnzXqj0RfG"
        "tuHghmv3aD-uzSYJ75XDdzKdizZ86IG6Fbn1XFhYZM-fbHhm3mVEXnyRW4ZuNOLFk"
        "Fas6LMcVC6Q8QLlHYbXBpdNFuGbuZGUnav5C-2I_-46lL0NGg3GewxGKGHvHEfoyn"
        "EFFlEYHsBQ98rXImL8ySDycdLEFvBPdtctPmWCfTxwmoSMLHU2SCVDhbqMWU5b0yr"
        "JBCScs_ejbKaqBDoB7ZGxTvqlrB__2ZmnHHjCr8RgMRtKNtIeuZAo ";
  }

  void Initialize(bool stream_should_process_data) {
    connection_ = new StrictMock<MockQuicConnection>(
        &helper_, &alarm_factory_, Perspective::IS_SERVER, supported_versions_);
    session_.reset(new StrictMock<MockQuicSession>(connection_));

    // New streams rely on having the peer's flow control receive window
    // negotiated in the config.
    QuicConfigPeer::SetReceivedInitialStreamFlowControlWindow(
        session_->config(), initial_flow_control_window_bytes_);

    stream_ = new TestStream(kTestStreamId, session_.get(),
                             stream_should_process_data);
    // session_ now owns stream_.
    session_->ActivateStream(QuicWrapUnique(stream_));
    // Ignore resetting when session_ is terminated.
    EXPECT_CALL(*session_, SendRstStream(kTestStreamId, _, _))
        .Times(AnyNumber());
    write_blocked_list_ =
        QuicSessionPeer::GetWriteBlockedStreams(session_.get());
    write_blocked_list_->RegisterStream(kTestStreamId, kV3HighestPriority);
  }

  bool fin_sent() { return QuicStreamPeer::FinSent(stream_); }
  bool rst_sent() { return QuicStreamPeer::RstSent(stream_); }

  void set_initial_flow_control_window_bytes(uint32_t val) {
    initial_flow_control_window_bytes_ = val;
  }

  bool HasWriteBlockedStreams() {
    return write_blocked_list_->HasWriteBlockedCryptoOrHeadersStream() ||
           write_blocked_list_->HasWriteBlockedDataStreams();
  }

  QuicConsumedData CloseStreamOnWriteError(
      QuicStream* /*stream*/,
      QuicStreamId id,
      QuicIOVector /*iov*/,
      QuicStreamOffset /*offset*/,
      StreamSendingState /*state*/,
      const QuicReferenceCountedPointer<
          QuicAckListenerInterface>& /*ack_listener*/) {
    session_->CloseStream(id);
    return QuicConsumedData(1, false);
  }

 protected:
  MockQuicConnectionHelper helper_;
  MockAlarmFactory alarm_factory_;
  MockQuicConnection* connection_;
  std::unique_ptr<MockQuicSession> session_;
  TestStream* stream_;
  SpdyHeaderBlock headers_;
  QuicWriteBlockedList* write_blocked_list_;
  uint32_t initial_flow_control_window_bytes_;
  QuicTime::Delta zero_;
  QuicTransportVersionVector supported_versions_;
  const QuicStreamId kTestStreamId = 5u;
};

TEST_F(QuicStreamTest, WriteAllData) {
  Initialize(kShouldProcessData);

  size_t length =
      1 + QuicPacketCreator::StreamFramePacketOverhead(
              connection_->transport_version(), PACKET_8BYTE_CONNECTION_ID,
              !kIncludeVersion, !kIncludeDiversificationNonce,
              PACKET_6BYTE_PACKET_NUMBER, 0u);
  connection_->SetMaxPacketLength(length);

  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(kDataLen, true)));
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_FALSE(HasWriteBlockedStreams());
}

TEST_F(QuicStreamTest, NoBlockingIfNoDataOrFin) {
  Initialize(kShouldProcessData);

  // Write no data and no fin.  If we consume nothing we should not be write
  // blocked.
  EXPECT_QUIC_BUG(stream_->WriteOrBufferData(QuicStringPiece(), false, nullptr),
                  "");
  EXPECT_FALSE(HasWriteBlockedStreams());
}

TEST_F(QuicStreamTest, BlockIfOnlySomeDataConsumed) {
  Initialize(kShouldProcessData);

  // Write some data and no fin.  If we consume some but not all of the data,
  // we should be write blocked a not all the data was consumed.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(1, false)));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 2), false, nullptr);
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
  EXPECT_EQ(1u, stream_->BufferedDataBytes());
}

TEST_F(QuicStreamTest, BlockIfFinNotConsumedWithData) {
  Initialize(kShouldProcessData);

  // Write some data and no fin.  If we consume all the data but not the fin,
  // we should be write blocked because the fin was not consumed.
  // (This should never actually happen as the fin should be sent out with the
  // last data)
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(2, false)));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 2), true, nullptr);
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
}

TEST_F(QuicStreamTest, BlockIfSoloFinNotConsumed) {
  Initialize(kShouldProcessData);

  // Write no data and a fin.  If we consume nothing we should be write blocked,
  // as the fin was not consumed.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, false)));
  stream_->WriteOrBufferData(QuicStringPiece(), true, nullptr);
  ASSERT_EQ(1u, write_blocked_list_->NumBlockedStreams());
}

TEST_F(QuicStreamTest, CloseOnPartialWrite) {
  Initialize(kShouldProcessData);

  // Write some data and no fin. However, while writing the data
  // close the stream and verify that MarkConnectionLevelWriteBlocked does not
  // crash with an unknown stream.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(Invoke(this, &QuicStreamTest::CloseStreamOnWriteError));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 2), false, nullptr);
  ASSERT_EQ(0u, write_blocked_list_->NumBlockedStreams());
}

TEST_F(QuicStreamTest, WriteOrBufferData) {
  Initialize(kShouldProcessData);

  EXPECT_FALSE(HasWriteBlockedStreams());
  size_t length =
      1 + QuicPacketCreator::StreamFramePacketOverhead(
              connection_->transport_version(), PACKET_8BYTE_CONNECTION_ID,
              !kIncludeVersion, !kIncludeDiversificationNonce,
              PACKET_6BYTE_PACKET_NUMBER, 0u);
  connection_->SetMaxPacketLength(length);

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(kDataLen - 1, false)));
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_EQ(1u, stream_->BufferedDataBytes());
  EXPECT_TRUE(HasWriteBlockedStreams());

  // Queue a bytes_consumed write.
  stream_->WriteOrBufferData(kData2, false, nullptr);
  EXPECT_EQ(10u, stream_->BufferedDataBytes());
  // Make sure we get the tail of the first write followed by the bytes_consumed
  InSequence s;
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(kDataLen - 1, false)));
  stream_->OnCanWrite();

  // And finally the end of the bytes_consumed.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(2, true)));
  stream_->OnCanWrite();
}

TEST_F(QuicStreamTest, ConnectionCloseAfterStreamClose) {
  Initialize(kShouldProcessData);

  QuicStreamPeer::CloseReadSide(stream_);
  stream_->CloseWriteSide();
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, stream_->connection_error());
  stream_->OnConnectionClosed(QUIC_INTERNAL_ERROR,
                              ConnectionCloseSource::FROM_SELF);
  EXPECT_EQ(QUIC_STREAM_NO_ERROR, stream_->stream_error());
  EXPECT_EQ(QUIC_NO_ERROR, stream_->connection_error());
}

TEST_F(QuicStreamTest, RstAlwaysSentIfNoFinSent) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if no FIN has been sent, we send a RST.

  Initialize(kShouldProcessData);
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Write some data, with no FIN.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(1, false)));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 1), false, nullptr);
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Now close the stream, and expect that we send a RST.
  EXPECT_CALL(*session_, SendRstStream(_, _, _));
  stream_->OnClose();
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());
}

TEST_F(QuicStreamTest, RstNotSentIfFinSent) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if a FIN has been sent, we don't also send a RST.

  Initialize(kShouldProcessData);
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Write some data, with FIN.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(1, true)));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 1), true, nullptr);
  EXPECT_TRUE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Now close the stream, and expect that we do not send a RST.
  stream_->OnClose();
  EXPECT_TRUE(fin_sent());
  EXPECT_FALSE(rst_sent());
}

TEST_F(QuicStreamTest, OnlySendOneRst) {
  // For flow control accounting, a stream must send either a FIN or a RST frame
  // before termination.
  // Test that if a stream sends a RST, it doesn't send an additional RST during
  // OnClose() (this shouldn't be harmful, but we shouldn't do it anyway...)

  Initialize(kShouldProcessData);
  EXPECT_FALSE(fin_sent());
  EXPECT_FALSE(rst_sent());

  // Reset the stream.
  const int expected_resets = 1;
  EXPECT_CALL(*session_, SendRstStream(_, _, _)).Times(expected_resets);
  stream_->Reset(QUIC_STREAM_CANCELLED);
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());

  // Now close the stream (any further resets being sent would break the
  // expectation above).
  stream_->OnClose();
  EXPECT_FALSE(fin_sent());
  EXPECT_TRUE(rst_sent());
}

TEST_F(QuicStreamTest, StreamFlowControlMultipleWindowUpdates) {
  set_initial_flow_control_window_bytes(1000);

  Initialize(kShouldProcessData);

  // If we receive multiple WINDOW_UPDATES (potentially out of order), then we
  // want to make sure we latch the largest offset we see.

  // Initially should be default.
  EXPECT_EQ(
      initial_flow_control_window_bytes_,
      QuicFlowControllerPeer::SendWindowOffset(stream_->flow_controller()));

  // Check a single WINDOW_UPDATE results in correct offset.
  QuicWindowUpdateFrame window_update_1(stream_->id(), 1234);
  stream_->OnWindowUpdateFrame(window_update_1);
  EXPECT_EQ(
      window_update_1.byte_offset,
      QuicFlowControllerPeer::SendWindowOffset(stream_->flow_controller()));

  // Now send a few more WINDOW_UPDATES and make sure that only the largest is
  // remembered.
  QuicWindowUpdateFrame window_update_2(stream_->id(), 1);
  QuicWindowUpdateFrame window_update_3(stream_->id(), 9999);
  QuicWindowUpdateFrame window_update_4(stream_->id(), 5678);
  stream_->OnWindowUpdateFrame(window_update_2);
  stream_->OnWindowUpdateFrame(window_update_3);
  stream_->OnWindowUpdateFrame(window_update_4);
  EXPECT_EQ(
      window_update_3.byte_offset,
      QuicFlowControllerPeer::SendWindowOffset(stream_->flow_controller()));
}

// TODO(ianswett): It's not clear this method is still needed now that
// ProxyAckNotifierDelegate has been removed.
void SaveAckListener(
    QuicReferenceCountedPointer<QuicAckListenerInterface>* ack_listener_out,
    QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener) {
  *ack_listener_out = std::move(ack_listener);
}

TEST_F(QuicStreamTest, WriteOrBufferDataWithQuicAckNotifier) {
  Initialize(kShouldProcessData);

  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);

  const int kDataSize = 16 * 1024;
  const string kData(kDataSize, 'a');

  const int kFirstWriteSize = 100;
  const int kSecondWriteSize = 50;
  const int kLastWriteSize = kDataSize - kFirstWriteSize - kSecondWriteSize;

  // Set a large flow control send window so this doesn't interfere with test.
  stream_->flow_controller()->UpdateSendWindowOffset(kDataSize + 1);
  session_->flow_controller()->UpdateSendWindowOffset(kDataSize + 1);

  QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener;

  EXPECT_CALL(*session_, WritevData(_, kTestStreamId, _, _, _, _))
      .WillOnce(DoAll(
          WithArgs<5>(Invoke(CreateFunctor(SaveAckListener, &ack_listener))),
          Return(QuicConsumedData(kFirstWriteSize, false))));
  stream_->WriteOrBufferData(kData, false, mock_ack_listener);
  EXPECT_TRUE(HasWriteBlockedStreams());

  EXPECT_CALL(*session_,
              WritevData(stream_, kTestStreamId, _, _, _, ack_listener))
      .WillOnce(Return(QuicConsumedData(kSecondWriteSize, false)));
  stream_->OnCanWrite();

  // No ack expected for an empty write.
  EXPECT_CALL(*session_,
              WritevData(stream_, kTestStreamId, _, _, _, ack_listener))
      .WillOnce(Return(QuicConsumedData(0, false)));
  stream_->OnCanWrite();

  EXPECT_CALL(*session_,
              WritevData(stream_, kTestStreamId, _, _, _, ack_listener))
      .WillOnce(Return(QuicConsumedData(kLastWriteSize, false)));
  stream_->OnCanWrite();
}

// Verify delegate behavior when packets are acked before the WritevData call
// that sends out the last byte.
TEST_F(QuicStreamTest, WriteOrBufferDataAckNotificationBeforeFlush) {
  Initialize(kShouldProcessData);

  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);

  const int kDataSize = 16 * 1024;
  const string kData(kDataSize, 'a');

  const int kInitialWriteSize = 100;

  // Set a large flow control send window so this doesn't interfere with test.
  stream_->flow_controller()->UpdateSendWindowOffset(kDataSize + 1);
  session_->flow_controller()->UpdateSendWindowOffset(kDataSize + 1);

  QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener;

  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(DoAll(
          WithArgs<5>(Invoke(CreateFunctor(SaveAckListener, &ack_listener))),
          Return(QuicConsumedData(kInitialWriteSize, false))));
  stream_->WriteOrBufferData(kData, false, mock_ack_listener);
  EXPECT_TRUE(HasWriteBlockedStreams());

  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(DoAll(
          WithArgs<5>(Invoke(CreateFunctor(SaveAckListener, &ack_listener))),
          Return(QuicConsumedData(kDataSize - kInitialWriteSize, false))));
  stream_->OnCanWrite();
}

// Verify delegate behavior when WriteOrBufferData does not buffer.
TEST_F(QuicStreamTest, WriteAndBufferDataWithAckNotiferNoBuffer) {
  Initialize(kShouldProcessData);

  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);

  QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener;

  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(DoAll(
          WithArgs<5>(Invoke(CreateFunctor(SaveAckListener, &ack_listener))),
          Return(QuicConsumedData(kDataLen, true))));
  stream_->WriteOrBufferData(kData1, true, mock_ack_listener);
  EXPECT_FALSE(HasWriteBlockedStreams());
}

// Verify delegate behavior when WriteOrBufferData buffers all the data.
TEST_F(QuicStreamTest, BufferOnWriteAndBufferDataWithAckNotifer) {
  Initialize(kShouldProcessData);

  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);

  QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener;

  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, false)));
  stream_->WriteOrBufferData(kData1, true, mock_ack_listener);
  EXPECT_TRUE(HasWriteBlockedStreams());

  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(DoAll(
          WithArgs<5>(Invoke(CreateFunctor(SaveAckListener, &ack_listener))),
          Return(QuicConsumedData(kDataLen, true))));
  stream_->OnCanWrite();
}

// Verify delegate behavior when WriteOrBufferData when the FIN is
// sent out in a different packet.
TEST_F(QuicStreamTest, WriteAndBufferDataWithAckNotiferOnlyFinRemains) {
  Initialize(kShouldProcessData);

  QuicReferenceCountedPointer<MockAckListener> mock_ack_listener(
      new StrictMock<MockAckListener>);

  QuicReferenceCountedPointer<QuicAckListenerInterface> ack_listener;

  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(DoAll(
          WithArgs<5>(Invoke(CreateFunctor(SaveAckListener, &ack_listener))),
          Return(QuicConsumedData(kDataLen, false))));
  stream_->WriteOrBufferData(kData1, true, mock_ack_listener);
  EXPECT_TRUE(HasWriteBlockedStreams());

  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(DoAll(
          WithArgs<5>(Invoke(CreateFunctor(SaveAckListener, &ack_listener))),
          Return(QuicConsumedData(0, true))));
  stream_->OnCanWrite();
}

// Verify that when we receive a packet which violates flow control (i.e. sends
// too much data on the stream) that the stream sequencer never sees this frame,
// as we check for violation and close the connection early.
TEST_F(QuicStreamTest, StreamSequencerNeverSeesPacketsViolatingFlowControl) {
  Initialize(kShouldProcessData);

  // Receive a stream frame that violates flow control: the byte offset is
  // higher than the receive window offset.
  QuicStreamFrame frame(stream_->id(), false,
                        kInitialSessionFlowControlWindowForTest + 1,
                        QuicStringPiece("."));
  EXPECT_GT(frame.offset, QuicFlowControllerPeer::ReceiveWindowOffset(
                              stream_->flow_controller()));

  // Stream should not accept the frame, and the connection should be closed.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _));
  stream_->OnStreamFrame(frame);
}

// Verify that after the consumer calls StopReading(), the stream still sends
// flow control updates.
TEST_F(QuicStreamTest, StopReadingSendsFlowControl) {
  Initialize(kShouldProcessData);

  stream_->StopReading();

  // Connection should not get terminated due to flow control errors.
  EXPECT_CALL(*connection_,
              CloseConnection(QUIC_FLOW_CONTROL_RECEIVED_TOO_MUCH_DATA, _, _))
      .Times(0);
  EXPECT_CALL(*connection_, SendWindowUpdate(_, _)).Times(AtLeast(1));

  string data(1000, 'x');
  for (QuicStreamOffset offset = 0;
       offset < 2 * kInitialStreamFlowControlWindowForTest;
       offset += data.length()) {
    QuicStreamFrame frame(stream_->id(), false, offset, data);
    stream_->OnStreamFrame(frame);
  }
  EXPECT_LT(
      kInitialStreamFlowControlWindowForTest,
      QuicFlowControllerPeer::ReceiveWindowOffset(stream_->flow_controller()));
}

TEST_F(QuicStreamTest, FinalByteOffsetFromFin) {
  Initialize(kShouldProcessData);

  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());

  QuicStreamFrame stream_frame_no_fin(stream_->id(), false, 1234,
                                      QuicStringPiece("."));
  stream_->OnStreamFrame(stream_frame_no_fin);
  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());

  QuicStreamFrame stream_frame_with_fin(stream_->id(), true, 1234,
                                        QuicStringPiece("."));
  stream_->OnStreamFrame(stream_frame_with_fin);
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
}

TEST_F(QuicStreamTest, FinalByteOffsetFromRst) {
  Initialize(kShouldProcessData);

  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());
  QuicRstStreamFrame rst_frame(stream_->id(), QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
}

TEST_F(QuicStreamTest, FinalByteOffsetFromZeroLengthStreamFrame) {
  // When receiving Trailers, an empty stream frame is created with the FIN set,
  // and is passed to OnStreamFrame. The Trailers may be sent in advance of
  // queued body bytes being sent, and thus the final byte offset may exceed
  // current flow control limits. Flow control should only be concerned with
  // data that has actually been sent/received, so verify that flow control
  // ignores such a stream frame.
  Initialize(kShouldProcessData);

  EXPECT_FALSE(stream_->HasFinalReceivedByteOffset());
  const QuicStreamOffset kByteOffsetExceedingFlowControlWindow =
      kInitialSessionFlowControlWindowForTest + 1;
  const QuicStreamOffset current_stream_flow_control_offset =
      QuicFlowControllerPeer::ReceiveWindowOffset(stream_->flow_controller());
  const QuicStreamOffset current_connection_flow_control_offset =
      QuicFlowControllerPeer::ReceiveWindowOffset(session_->flow_controller());
  ASSERT_GT(kByteOffsetExceedingFlowControlWindow,
            current_stream_flow_control_offset);
  ASSERT_GT(kByteOffsetExceedingFlowControlWindow,
            current_connection_flow_control_offset);
  QuicStreamFrame zero_length_stream_frame_with_fin(
      stream_->id(), /*fin=*/true, kByteOffsetExceedingFlowControlWindow,
      QuicStringPiece());
  EXPECT_EQ(0, zero_length_stream_frame_with_fin.data_length);

  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  stream_->OnStreamFrame(zero_length_stream_frame_with_fin);
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());

  // The flow control receive offset values should not have changed.
  EXPECT_EQ(
      current_stream_flow_control_offset,
      QuicFlowControllerPeer::ReceiveWindowOffset(stream_->flow_controller()));
  EXPECT_EQ(
      current_connection_flow_control_offset,
      QuicFlowControllerPeer::ReceiveWindowOffset(session_->flow_controller()));
}

TEST_F(QuicStreamTest, SetDrainingIncomingOutgoing) {
  // Don't have incoming data consumed.
  Initialize(kShouldNotProcessData);

  // Incoming data with FIN.
  QuicStreamFrame stream_frame_with_fin(stream_->id(), true, 1234,
                                        QuicStringPiece("."));
  stream_->OnStreamFrame(stream_frame_with_fin);
  // The FIN has been received but not consumed.
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_FALSE(stream_->reading_stopped());

  EXPECT_EQ(1u, session_->GetNumOpenIncomingStreams());

  // Outgoing data with FIN.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(2, true)));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 2), true, nullptr);
  EXPECT_TRUE(stream_->write_side_closed());

  EXPECT_EQ(1u, QuicSessionPeer::GetDrainingStreams(session_.get())
                    ->count(kTestStreamId));
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());
}

TEST_F(QuicStreamTest, SetDrainingOutgoingIncoming) {
  // Don't have incoming data consumed.
  Initialize(kShouldNotProcessData);

  // Outgoing data with FIN.
  EXPECT_CALL(*session_, WritevData(stream_, kTestStreamId, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(2, true)));
  stream_->WriteOrBufferData(QuicStringPiece(kData1, 2), true, nullptr);
  EXPECT_TRUE(stream_->write_side_closed());

  EXPECT_EQ(1u, session_->GetNumOpenIncomingStreams());

  // Incoming data with FIN.
  QuicStreamFrame stream_frame_with_fin(stream_->id(), true, 1234,
                                        QuicStringPiece("."));
  stream_->OnStreamFrame(stream_frame_with_fin);
  // The FIN has been received but not consumed.
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
  EXPECT_FALSE(QuicStreamPeer::read_side_closed(stream_));
  EXPECT_FALSE(stream_->reading_stopped());

  EXPECT_EQ(1u, QuicSessionPeer::GetDrainingStreams(session_.get())
                    ->count(kTestStreamId));
  EXPECT_EQ(0u, session_->GetNumOpenIncomingStreams());
}

TEST_F(QuicStreamTest, EarlyResponseFinHandling) {
  // Verify that if the server completes the response before reading the end of
  // the request, the received FIN is recorded.

  Initialize(kShouldProcessData);
  EXPECT_CALL(*connection_, CloseConnection(_, _, _)).Times(0);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeAllData));

  // Receive data for the request.
  QuicStreamFrame frame1(stream_->id(), false, 0, QuicStringPiece("Start"));
  stream_->OnStreamFrame(frame1);
  // When QuicSimpleServerStream sends the response, it calls
  // QuicStream::CloseReadSide() first.
  QuicStreamPeer::CloseReadSide(stream_);
  // Send data and FIN for the response.
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_TRUE(QuicStreamPeer::read_side_closed(stream_));
  // Receive remaining data and FIN for the request.
  QuicStreamFrame frame2(stream_->id(), true, 0, QuicStringPiece("End"));
  stream_->OnStreamFrame(frame2);
  EXPECT_TRUE(stream_->fin_received());
  EXPECT_TRUE(stream_->HasFinalReceivedByteOffset());
}

TEST_F(QuicStreamTest, StreamWaitsForAcks) {
  Initialize(kShouldProcessData);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeAllData));
  // Stream is not waiting for acks initially.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  // Send kData1.
  stream_->WriteOrBufferData(kData1, false, nullptr);
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  QuicStreamFrame frame1(stream_->id(), false, 0, kData1);
  stream_->OnStreamFrameAcked(frame1, QuicTime::Delta::Zero());
  // Stream is not waiting for acks as all sent data is acked.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  // Send kData2.
  stream_->WriteOrBufferData(kData2, false, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Send FIN.
  stream_->WriteOrBufferData("", true, nullptr);
  // Fin only frame is not stored in send buffer.
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());

  // kData2 is acked.
  QuicStreamFrame frame2(stream_->id(), false, 9, kData2);
  stream_->OnStreamFrameAcked(frame2, QuicTime::Delta::Zero());
  // Stream is waiting for acks as FIN is not acked.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  // FIN is acked.
  QuicStreamFrame frame3(stream_->id(), true, 18, "");
  stream_->OnStreamFrameAcked(frame3, QuicTime::Delta::Zero());
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
}

TEST_F(QuicStreamTest, StreamDataGetAckedOutOfOrder) {
  Initialize(kShouldProcessData);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeAllData));
  // Send data.
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData(kData1, false, nullptr);
  stream_->WriteOrBufferData("", true, nullptr);
  EXPECT_EQ(3u, QuicStreamPeer::SendBuffer(stream_).size());
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  QuicStreamFrame frame1(stream_->id(), false, 0, kData1);
  QuicStreamFrame frame2(stream_->id(), false, 9, kData1);
  QuicStreamFrame frame3(stream_->id(), false, 18, kData1);
  QuicStreamFrame frame4(stream_->id(), true, 27, "");
  stream_->OnStreamFrameAcked(frame2, QuicTime::Delta::Zero());
  EXPECT_EQ(3u, QuicStreamPeer::SendBuffer(stream_).size());
  stream_->OnStreamFrameAcked(frame3, QuicTime::Delta::Zero());
  EXPECT_EQ(3u, QuicStreamPeer::SendBuffer(stream_).size());
  stream_->OnStreamFrameDiscarded(frame1);
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
  // FIN is not acked yet.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  stream_->OnStreamFrameAcked(frame4, QuicTime::Delta::Zero());
  EXPECT_FALSE(stream_->IsWaitingForAcks());
}

TEST_F(QuicStreamTest, CancelStream) {
  Initialize(kShouldProcessData);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeAllData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, false, nullptr);
  QuicStreamFrame frame(stream_->id(), 0, false, kData1);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
  // Cancel stream.
  stream_->Reset(QUIC_STREAM_NO_ERROR);
  // stream still waits for acks as the error code is QUIC_STREAM_NO_ERROR, and
  // data is going to be retransmitted.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_CALL(*session_,
              SendRstStream(stream_->id(), QUIC_STREAM_CANCELLED, 9));
  stream_->Reset(QUIC_STREAM_CANCELLED);
  stream_->OnStreamFrameDiscarded(frame);
  // Stream stops waiting for acks as data is not going to be retransmitted.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
}

TEST_F(QuicStreamTest, RstFrameReceivedStreamNotFinishSending) {
  Initialize(kShouldProcessData);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeAllData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, false, nullptr);
  QuicStreamFrame frame(stream_->id(), 0, false, kData1);
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());

  // RST_STREAM received.
  QuicRstStreamFrame rst_frame(stream_->id(), QUIC_STREAM_CANCELLED, 9);
  EXPECT_CALL(*session_,
              SendRstStream(stream_->id(), QUIC_RST_ACKNOWLEDGEMENT, 9));
  stream_->OnStreamReset(rst_frame);
  stream_->OnStreamFrameDiscarded(frame);
  // Stream stops waiting for acks as it does not finish sending and rst is
  // sent.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
}

TEST_F(QuicStreamTest, RstFrameReceivedStreamFinishSending) {
  Initialize(kShouldProcessData);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeAllData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, true, nullptr);
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  // RST_STREAM received.
  EXPECT_CALL(*session_, SendRstStream(_, _, _)).Times(0);
  QuicRstStreamFrame rst_frame(stream_->id(), QUIC_STREAM_CANCELLED, 1234);
  stream_->OnStreamReset(rst_frame);
  // Stream stops waiting for acks as it has unacked data.
  EXPECT_TRUE(stream_->IsWaitingForAcks());
  EXPECT_EQ(1u, QuicStreamPeer::SendBuffer(stream_).size());
}

TEST_F(QuicStreamTest, ConnectionClosed) {
  Initialize(kShouldProcessData);
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillRepeatedly(Invoke(MockQuicSession::ConsumeAllData));
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());

  stream_->WriteOrBufferData(kData1, false, nullptr);
  QuicStreamFrame frame(stream_->id(), 0, false, kData1);
  EXPECT_TRUE(stream_->IsWaitingForAcks());

  EXPECT_CALL(*session_,
              SendRstStream(stream_->id(), QUIC_RST_ACKNOWLEDGEMENT, 9));
  stream_->OnConnectionClosed(QUIC_INTERNAL_ERROR,
                              ConnectionCloseSource::FROM_SELF);
  stream_->OnStreamFrameDiscarded(frame);
  // Stream stops waiting for acks as connection is going to close.
  EXPECT_FALSE(stream_->IsWaitingForAcks());
  EXPECT_EQ(0u, QuicStreamPeer::SendBuffer(stream_).size());
}

TEST_F(QuicStreamTest, WriteBufferedData) {
  // Set buffered data low water mark to be 100.
  SetQuicFlag(&FLAGS_quic_buffered_data_threshold, 100);
  // Do not stream level flow control block this stream.
  set_initial_flow_control_window_bytes(500000);

  Initialize(kShouldProcessData);
  string data(1024, 'a');
  EXPECT_TRUE(stream_->CanWriteNewData());

  // Testing WriteOrBufferData.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  stream_->WriteOrBufferData(data, false, nullptr);
  stream_->WriteOrBufferData(data, false, nullptr);
  stream_->WriteOrBufferData(data, false, nullptr);
  // Verify all data is saved.
  EXPECT_EQ(3 * data.length() - 100, stream_->BufferedDataBytes());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  // Buffered data size > threshold, do not ask upper layer for more data.
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(0);
  stream_->OnCanWrite();
  EXPECT_EQ(3 * data.length() - 200, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->CanWriteNewData());

  // Send buffered data to make buffered data size < threshold.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(
          3 * data.length() - 200 -
              GetQuicFlag(FLAGS_quic_buffered_data_threshold) + 1,
          false)));
  // Buffered data size < threshold, ask upper layer for more data.
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(GetQuicFlag(FLAGS_quic_buffered_data_threshold) - 1u,
            stream_->BufferedDataBytes());
  EXPECT_TRUE(stream_->CanWriteNewData());

  // Flush all buffered data.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeAllData));
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(0u, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->HasBufferedData());
  EXPECT_TRUE(stream_->CanWriteNewData());

  // Testing Writev.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(0, false)));
  struct iovec iov = {const_cast<char*>(data.data()), data.length()};
  QuicConsumedData consumed = stream_->WritevData(&iov, 1, false, nullptr);
  // There is no buffered data before, all data should be consumed without
  // respecting buffered data upper limit.
  EXPECT_EQ(data.length(), consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(data.length(), stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->CanWriteNewData());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).Times(0);
  consumed = stream_->WritevData(&iov, 1, false, nullptr);
  // No Data can be consumed as buffered data is beyond upper limit.
  EXPECT_EQ(0u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(data.length(), stream_->BufferedDataBytes());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(
          data.length() - FLAGS_quic_buffered_data_threshold + 1, false)));
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(GetQuicFlag(FLAGS_quic_buffered_data_threshold) - 1,
            stream_->BufferedDataBytes());
  EXPECT_TRUE(stream_->CanWriteNewData());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).Times(0);
  // All data can be consumed as buffered data is below upper limit.
  consumed = stream_->WritevData(&iov, 1, false, nullptr);
  EXPECT_EQ(data.length(), consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(data.length() + GetQuicFlag(FLAGS_quic_buffered_data_threshold) - 1,
            stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->CanWriteNewData());
}

TEST_F(QuicStreamTest, WriteMemSlices) {
  // Set buffered data low water mark to be 100.
  SetQuicFlag(&FLAGS_quic_buffered_data_threshold, 100);
  // Do not flow control block this stream.
  set_initial_flow_control_window_bytes(500000);

  Initialize(kShouldProcessData);
  if (!session_->can_use_slices()) {
    return;
  }
  char data[1024];
  std::vector<std::pair<char*, int>> buffers;
  buffers.push_back(std::make_pair(data, arraysize(data)));
  buffers.push_back(std::make_pair(data, arraysize(data)));
  QuicTestMemSliceVector vector1(buffers);
  QuicTestMemSliceVector vector2(buffers);
  QuicMemSliceSpan span1 = vector1.span();
  QuicMemSliceSpan span2 = vector2.span();

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(100, false)));
  // There is no buffered data before, all data should be consumed.
  QuicConsumedData consumed = stream_->WriteMemSlices(span1, false);
  EXPECT_EQ(2048u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(2 * arraysize(data) - 100, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->fin_buffered());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).Times(0);
  // No Data can be consumed as buffered data is beyond upper limit.
  consumed = stream_->WriteMemSlices(span2, true);
  EXPECT_EQ(0u, consumed.bytes_consumed);
  EXPECT_FALSE(consumed.fin_consumed);
  EXPECT_EQ(2 * arraysize(data) - 100, stream_->BufferedDataBytes());
  EXPECT_FALSE(stream_->fin_buffered());

  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Return(QuicConsumedData(
          2 * arraysize(data) - 100 -
              GetQuicFlag(FLAGS_quic_buffered_data_threshold) + 1,
          false)));
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(1);
  stream_->OnCanWrite();
  EXPECT_EQ(GetQuicFlag(FLAGS_quic_buffered_data_threshold) - 1,
            stream_->BufferedDataBytes());
  // Try to write slices2 again.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _)).Times(0);
  consumed = stream_->WriteMemSlices(span2, true);
  EXPECT_EQ(2048u, consumed.bytes_consumed);
  EXPECT_TRUE(consumed.fin_consumed);
  EXPECT_EQ(
      2 * arraysize(data) + GetQuicFlag(FLAGS_quic_buffered_data_threshold) - 1,
      stream_->BufferedDataBytes());
  EXPECT_TRUE(stream_->fin_buffered());

  // Flush all buffered data.
  EXPECT_CALL(*session_, WritevData(_, _, _, _, _, _))
      .WillOnce(Invoke(MockQuicSession::ConsumeAllData));
  stream_->OnCanWrite();
  EXPECT_CALL(*stream_, OnCanWriteNewData()).Times(0);
  EXPECT_FALSE(stream_->HasBufferedData());
  EXPECT_TRUE(stream_->write_side_closed());
}

}  // namespace
}  // namespace test
}  // namespace net
