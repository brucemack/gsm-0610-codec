Overview
========

A GSM 06.10 CODEC for plain, simple folks. GSM 06.10 is the classic "full rate" GSM specification.  This project provides an implementation of the encoder and the decoder that is suitable for embedded 
processors.

Key Features
============

* A completely fixed-point (integer) implementation for efficiency on simple hardware.
* No external dependencies.
* Provides optional support for homing.

References
==========

The most important reference is the "official" specification document published by the European Telecommunications Standards Institute.  The latest version I could find is V8.0.1 located here: https://www.etsi.org/deliver/etsi_EN/300900_300999/300961/08.00.01_40/en_300961v080001o.pdf.

There is also an important data directory that contains the official tests sequences located here: https://www.etsi.org/deliver/etsi_en/300900_300999/300961/08.00.01_40/

Background on Issai Shur: https://en.wikipedia.org/wiki/Issai_Schur

The method of packing a GSM frame into an RTP packet is described in RFC 3551.  There may be other 
formats used in other contexts, so support may need to be expanded.  The RFC is here: https://datatracker.ietf.org/doc/html/rfc3551#section-4.5.8.1

Licensing 
=========

This software is provided under the GNU GPL V3 license.  However, a commercial 
license is also available.  Please contact bruce at mackinnon dot com for more
information.

License Statement
=================

GSM 06.10 CODEC
Copyright (C) 2024, Bruce MacKinnon 

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

NOT FOR COMMERCIAL USE WITHOUT PERMISSION.
