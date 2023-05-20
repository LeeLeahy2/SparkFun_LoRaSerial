/*
    Drop [8, 8]

             12345678
            .--------.
        0x01|    *   |
        0x02|   ***  |
        0x04|  ***** |
        0x08|  ***** |
        0x10| *******|
        0x20| *******|
        0x40| *******|
        0x80|  ***** |
            '--------'
*/

const int Drop_Width = 8;
const int Drop_Height = 8;
const uint8_t Drop [] = {
  0x00, 0x70, 0xfc, 0xfe, 0xff, 0xfe, 0xfc, 0x70
};

/*
    DownloadArrow [8, 8]

             12345678
            .--------.
        0x01|   **   |
        0x02|   **   |
        0x04|   **   |
        0x08|   **   |
        0x10|** ** **|
        0x20| ****** |
        0x40|  ****  |
        0x80|   **   |
            '--------'
*/

const int DownArrow_Width = 8;
const int DownArrow_Height = 8;
const uint8_t DownArrow [] = {
  0x10, 0x30, 0x60, 0xff, 0xff, 0x60, 0x30, 0x10
};

/*
    UploadArrow [8, 8]

             12345678
            .--------.
        0x01|   **   |
        0x02|  ****  |
        0x04| ****** |
        0x08|** ** **|
        0x10|   **   |
        0x20|   **   |
        0x40|   **   |
        0x80|   **   |
            '--------'
*/

const int UpArrow_Width = 8;
const int UpArrow_Height = 8;
const uint8_t UpArrow [] = {
  0x08, 0x0c, 0x06, 0xff, 0x0f, 0x06, 0x0c, 0x08
};
