/* PrintStream.java -- OutputStream for printing output
   Copyright (C) 1998,2003 Free Software Foundation, Inc.

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.
 
GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA.

Linking this library statically or dynamically with other modules is
making a combined work based on this library.  Thus, the terms and
conditions of the GNU General Public License cover the whole
combination.

As a special exception, the copyright holders of this library give you
permission to link this library with independent modules to produce an
executable, regardless of the license terms of these independent
modules, and to copy and distribute the resulting executable under
terms of your choice, provided that you also meet, for each linked
independent module, the terms and conditions of the license of that
module.  An independent module is a module which is not derived from
or based on this library.  If you modify this library, you may extend
this exception to your version of the library, but you are not
obligated to do so.  If you do not wish to do so, delete this
exception statement from your version. */


package java.io;
import gnu.gcj.convert.UnicodeToBytes;

/**
 * @author Tom Tromey <tromey@cygnus.com>
 * @date September 24, 1998 
 */

/* Written using "Java Class Libraries", 2nd edition, ISBN 0-201-31002-3
 * "The Java Language Specification", ISBN 0-201-63451-1
 * Status:  Believed complete and correct to 1.3
 */

public class PrintStream extends FilterOutputStream
{
  /* Notice the implementation is quite similar to OutputStreamWriter.
   * This leads to some minor duplication, because neither inherits
   * from the other, and we want to maximize performance. */

  public boolean checkError ()
  {
    flush();
    return error;
  }

  public void close ()
  {
    try
      {
	flush();
	out.close();
      }
    catch (InterruptedIOException iioe)
      {
	Thread.currentThread().interrupt();
      }
    catch (IOException e)
      {
	setError ();
      }
  }

  public void flush ()
  {
    try
      {
	out.flush();
      }
    catch (InterruptedIOException iioe)
      {
	Thread.currentThread().interrupt();
      }
    catch (IOException e)
      {
	setError ();
      }
  }

  private synchronized void print (String str, boolean println)
  {
    try
      {
        writeChars(str, 0, str.length());
	if (println)
	  writeChars(line_separator, 0, line_separator.length);
	if (auto_flush)
	  flush();
      }
    catch (InterruptedIOException iioe)
      {
	Thread.currentThread().interrupt();
      }
    catch (IOException e)
      {
	setError ();
      }
  }

  private synchronized void print (char[] chars, int pos, int len,
				   boolean println)
  {
    try
      {
        writeChars(chars, pos, len);
	if (println)
	  writeChars(line_separator, 0, line_separator.length);
	if (auto_flush)
	  flush();
      }
    catch (InterruptedIOException iioe)
      {
	Thread.currentThread().interrupt();
      }
    catch (IOException e)
      {
	setError ();
      }
  }

  private void writeChars(char[] buf, int offset, int count)
    throws IOException
  {
    while (count > 0)
      {
	converter.setOutput(work_bytes, 0);
	int converted = converter.write(buf, offset, count);
	offset += converted;
	count -= converted;
	out.write(work_bytes, 0, converter.count);
      }
  }

  private void writeChars(String str, int offset, int count)
    throws IOException
  {
    while (count > 0)
      {
	converter.setOutput(work_bytes, 0);
	int converted = converter.write(str, offset, count, work);
	offset += converted;
	count -= converted;
	out.write(work_bytes, 0, converter.count);
      }
  }

  public void print (boolean bool)
  {
    print(String.valueOf(bool), false);
  }

  public void print (int inum)
  {
    print(String.valueOf(inum), false);
  }

  public void print (long lnum)
  {
    print(String.valueOf(lnum), false);
  }

  public void print (float fnum)
  {
    print(String.valueOf(fnum), false);
  }

  public void print (double dnum)
  {
    print(String.valueOf(dnum), false);
  }

  public void print (Object obj)
  {
    print(obj == null ? "null" : obj.toString(), false);
  }

  public void print (String str)
  {
    print(str == null ? "null" : str, false);
  }

  public synchronized void print (char ch)
  {
    work[0] = ch;
    print(work, 0, 1, false);
  }

  public void print (char[] charArray)
  {
    print(charArray, 0, charArray.length, false);
  }

  public void println ()
  {
    print(line_separator, 0, line_separator.length, false);
  }

  public void println (boolean bool)
  {
    print(String.valueOf(bool), true);
  }

  public void println (int inum)
  {
    print(String.valueOf(inum), true);
  }

  public void println (long lnum)
  {
    print(String.valueOf(lnum), true);
  }

  public void println (float fnum)
  {
    print(String.valueOf(fnum), true);
  }

  public void println (double dnum)
  {
    print(String.valueOf(dnum), true);
  }

  public void println (Object obj)
  {
    print(obj == null ? "null" : obj.toString(), true);
  }

  public void println (String str)
  {
    print (str == null ? "null" : str, true);
  }

  public synchronized void println (char ch)
  {
    work[0] = ch;
    print(work, 0, 1, true);
  }

  public void println (char[] charArray)
  {
    print(charArray, 0, charArray.length, true);
  }

  public PrintStream (OutputStream out)
  {
    this(out, false);
  }

  public PrintStream (OutputStream out, boolean af)
  {
    super(out);
    converter = UnicodeToBytes.getDefaultEncoder();
    error = false;
    auto_flush = af;
  }

  protected void setError ()
  {
    error = true;
  }

  public void write (int oneByte)
  {
    try
      {
	out.write(oneByte);
	if (auto_flush && oneByte == '\n')
	  flush();
      }
    catch (InterruptedIOException iioe)
      {
	Thread.currentThread().interrupt();
      }
    catch (IOException e)
      {
	setError ();
      }
  }

  public void write (byte[] buffer, int offset, int count)
  {
    try
      {
	out.write(buffer, offset, count);
	if (auto_flush)
	  flush();
      }
    catch (InterruptedIOException iioe)
      {
	Thread.currentThread().interrupt();
      }
    catch (IOException e)
      {
	setError ();
      }
  }

  UnicodeToBytes converter;

  // Work buffer of characters for converter.
  char[] work = new char[100];
  // Work buffer of bytes where we temporarily keep converter output.
  byte[] work_bytes = new byte[100];

  // True if error occurred.
  private boolean error;
  // True if auto-flush.
  private boolean auto_flush;

  // Line separator string.
  private static final char[] line_separator
    = System.getProperty("line.separator").toCharArray();
}
