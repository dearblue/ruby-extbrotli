#vim: set fileencoding:utf-8

ver = RUBY_VERSION.slice(/\d+\.\d+/)
soname = File.basename(__FILE__, ".rb") << ".so"
lib = File.join(File.dirname(__FILE__), ver, soname)
case
when File.file?(lib)
  require_relative File.join(ver, soname)
when File.file?(File.join(File.dirname(__FILE__), ver))
  require_relative soname
else
  require soname
end

require "stringio"
require_relative "extbrotli/version"

module Brotli
  Brotli = self

  #
  # call-seq:
  #   encode(**opts) -> brotli encoder
  #   encode(**opts) { |encoder| ... } -> outport
  #   encode(string, **opts) -> brotli'd string data
  #   encode(outport, **opts) -> brotli encoder
  #   encode(outport, **opts) { |encoder| ... } -> outport
  #
  def self.encode(outport = nil, **opts)
    if outport.kind_of?(String)
      return LowLevelEncoder.encode(outport, **opts)
    end

    e = Encoder.new(outport || "".b, **opts)
    return e unless block_given?

    begin
      yield(e)
      e.outport
    ensure
      e.close
    end
  end

  #
  # call-seq:
  #   decode(brotli_string) -> decoded string data
  #   decode(inport) -> brotli decoder
  #   decode(inport) { |decoder| ... } -> yield return
  #
  def self.decode(inport)
    if inport.kind_of?(String)
      # NOTE: LowLevelDecoder.decode を使わないのは、destsize の計算のための
      # NOTE: BrotliDecompressedSize() が全体としてのバイト数を返さないため
      return decode(StringIO.new(inport)) { |d| d.read }
    end

    d = Decoder.new(inport)
    return d unless block_given?

    begin
      yield(d)
    ensure
      d.close
    end
  end

  class Encoder
    attr :encoder, :outport, :writebuf, :outbuf, :status

    def initialize(outport, **opts)
      @encoder = encoder = LowLevelEncoder.new(**opts)
      @outport = outport
      @writebuf = writebuf = StringIO.new("".b)
      @outbuf = outbuf = "".b
      @status = status = [:ready] # for finalizer

      enc = self
      self.class.class_eval do
        set_finalizer(enc, encoder, outport, writebuf, outbuf, status)
      end
    end

    def eof
      status[0] == :ready ? false : true
    end

    alias eof? eof

    def write(buf)
      raise "closed stream" unless status

      writebuf << buf
      w = ""
      blocksize = encoder.blocksize
      blocks = writebuf.size / blocksize
      if blocks > 0
        writebuf.rewind
        blocks.times do |i|
          writebuf.read(blocksize, w)
          encoder.encode_metablock(w, outbuf, LowLevelEncoder.compress_bound(blocksize), false);
          outport << outbuf unless outbuf.empty?
        end
        writebuf.read(blocksize, w)
        writebuf.truncate(0)
        writebuf.rewind
        writebuf << w
      end

      self
    end

    alias << write

    def close
      raise "closed stream" unless status

      blocksize = encoder.blocksize

      if writebuf.size > 0
        encoder.encode_metablock(writebuf.string, outbuf, LowLevelEncoder.compress_bound(blocksize), true)
      else
        encoder.finish(outbuf, LowLevelEncoder.compress_bound(blocksize))
      end

      outport << outbuf unless outbuf.empty?

      status[0] = nil

      nil
    end

    class << self
      private
      def set_finalizer(obj, encoder, outport, writebuf, outbuf, status)
        block = make_finalizer_block(encoder, outport, writebuf, outbuf, status)
        ObjectSpace.define_finalizer(obj, block)
        nil
      end

      private
      def make_finalizer_block(encoder, outport, writebuf, outbuf, status)
        proc do
          if status[0] == :ready
            TODO writebuf を flush する
            TODO encoder.finish する

            encoder.finish(outbuf, 1024 * 1024) # FIXME: retry to up scale size
            outport << outbuf
          end
        end
      end
    end
  end

  class Decoder
    attr :decoder, :inport, :readbuf, :destbuf, :status

    BLOCKSIZE = 256 * 1024

    def initialize(inport)
      @decoder = LowLevelDecoder.new
      @inport = inport
      @readbuf = "".b
      @destbuf = StringIO.new("".b)
      @status = true # true, nil
    end

    def read(size = nil, buf = nil)
      buf ||= "".b
      buf.clear
      return buf if size == 0
      return nil unless status
      w = "".b
      while size.nil? || size > 0
        if destbuf.eof? && !fetch
          @status = nil
          break
        end

        if size
          s = destbuf.size - destbuf.pos
          s = size if s > size
          break unless destbuf.read(s, w)
          buf << w
          size -= w.bytesize
        else
          break unless destbuf.read(destbuf.size, w)
          buf << w
        end
      end

      if buf.empty?
        nil
      else
        buf
      end
    end

    def close
      @status = nil
    end

    def eof
      !status
    end

    alias eof? eof

    private
    def fetch
      return nil if eof? || status == :done
      return self unless destbuf.eof?
      destbuf.string.clear
      destbuf.rewind
      d = "".b
      s = 0
      until destbuf.size > 0
        if readbuf.empty?
          if !inport.eof && t = inport.read(BLOCKSIZE)
            readbuf << t
          else
            finish = true
          end
        end
        (s, r, d) = decoder.decode(readbuf, d, BLOCKSIZE, finish ? 1 : 0)
        if r
          @readbuf = r
        else
          readbuf.clear
        end
        destbuf << d if d
        case s
        when 1
          @status = :done
          break
        end
      end
      destbuf.rewind
      self
    end
  end
end
