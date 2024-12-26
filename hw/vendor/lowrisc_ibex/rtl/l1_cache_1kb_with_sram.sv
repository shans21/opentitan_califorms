module direct_mapped_cache (
  input wire clk,
  input wire reset,
  input wire [31:0] cpu_address,
  input wire [511:0] cpu_write_data,
  input wire cpu_write_enable,
  input wire cpu_read_enable,
  output reg [511:0] cpu_read_data,
  output reg cpu_ready,

  // Memory interface
  output reg [31:0] mem_address,
  output reg [511:0] mem_write_data,
  output reg mem_write_enable,
  output reg mem_read_enable,
  input wire [511:0] mem_read_data,
  input wire mem_ready
);

  // Cache parameters
  parameter CACHE_SIZE = 1024; // 1KB
  parameter LINE_SIZE = 64; // 64-byte line
  parameter NUM_LINES = CACHE_SIZE / LINE_SIZE; // 16 lines

  // Address breakdown
  wire [3:0] index = cpu_address[9:6];
  wire [21:0] tag = cpu_address[31:10];
  wire [5:0] offset = cpu_address[5:0];

  // Cache states
  localparam IDLE = 2'b00;
  localparam READ_MISS = 2'b01;
  localparam WRITE_THROUGH = 2'b10;

  reg [1:0] state;

  // Valid bit storage
  reg valid [0:NUM_LINES-1];

  // Data SRAM signals
  reg data_sram_csb0, data_sram_csb1;
  reg [3:0] data_sram_addr0, data_sram_addr1;
  reg [511:0] data_sram_din0;
  wire [511:0] data_sram_dout1;

  // Tag SRAM signals
  reg tag_sram_csb0, tag_sram_csb1;
  reg [3:0] tag_sram_addr0, tag_sram_addr1;
  reg [21:0] tag_sram_din0;
  wire [21:0] tag_sram_dout1;

  // Instantiate the Data SRAM
  sram_0rw1r1w_512_16_freepdk45 data_sram (
    .clk0(clk),
    .csb0(data_sram_csb0),
    .addr0(data_sram_addr0),
    .din0(data_sram_din0),
    .clk1(clk),
    .csb1(data_sram_csb1),
    .addr1(data_sram_addr1),
    .dout1(data_sram_dout1)
  );

  // Instantiate the Tag SRAM
  sram_0rw1r1w_22_16_freepdk45 tag_sram (
    .clk0(clk),
    .csb0(tag_sram_csb0),
    .addr0(tag_sram_addr0),
    .din0(tag_sram_din0),
    .clk1(clk),
    .csb1(tag_sram_csb1),
    .addr1(tag_sram_addr1),
    .dout1(tag_sram_dout1)
  );

  // Cache operation
  always @(posedge clk or posedge reset) begin
    if (reset) begin
      integer i;
      for (i = 0; i < NUM_LINES; i = i + 1) begin
        valid[i] <= 0;
      end
      cpu_ready <= 1;
      state <= IDLE;
      mem_write_enable <= 0;
      mem_read_enable <= 0;
      data_sram_csb0 <= 1;
      data_sram_csb1 <= 1;
      tag_sram_csb0 <= 1;
      tag_sram_csb1 <= 1;
    end else begin
      case (state)
        IDLE: begin
          if (cpu_read_enable) begin
            tag_sram_csb1 <= 0;
            tag_sram_addr1 <= index;
            data_sram_csb1 <= 0;
            data_sram_addr1 <= index;
            if (valid[index] && tag_sram_dout1 == tag) begin
              // Cache hit
              cpu_read_data <= data_sram_dout1;
              cpu_ready <= 1;
            end else begin
              // Cache miss
              state <= READ_MISS;
              mem_address <= {cpu_address[31:6], 6'b0}; // Align to 64-byte boundary
              mem_read_enable <= 1;
              cpu_ready <= 0;
            end
          end else if (cpu_write_enable) begin
            // Write-through: update both cache and memory
            data_sram_csb0 <= 0;
            data_sram_addr0 <= index;
            data_sram_din0 <= cpu_write_data;
            tag_sram_csb0 <= 0;
            tag_sram_addr0 <= index;
            tag_sram_din0 <= tag;
            valid[index] <= 1;
            state <= WRITE_THROUGH;
            mem_address <= cpu_address;
            mem_write_data <= cpu_write_data;
            mem_write_enable <= 1;
            cpu_ready <= 0;
          end else begin
            data_sram_csb0 <= 1;
            data_sram_csb1 <= 1;
            tag_sram_csb0 <= 1;
            tag_sram_csb1 <= 1;
          end
        end

        READ_MISS: begin
          if (mem_ready) begin
            data_sram_csb0 <= 0;
            data_sram_addr0 <= index;
            data_sram_din0 <= mem_read_data;
            tag_sram_csb0 <= 0;
            tag_sram_addr0 <= index;
            tag_sram_din0 <= tag;
            valid[index] <= 1;
            cpu_read_data <= mem_read_data;
            state <= IDLE;
            mem_read_enable <= 0;
            cpu_ready <= 1;
          end
        end

        WRITE_THROUGH: begin
          if (mem_ready) begin
            state <= IDLE;
            mem_write_enable <= 0;
            cpu_ready <= 1;
            data_sram_csb0 <= 1;
            tag_sram_csb0 <= 1;
          end
        end
      endcase
    end
  end

endmodule

