/*
 * Anatomy.cpp
 *
 *  Created on: 07.07.2008
 *      Author: ralph
 */

#include "Anatomy.h"

#include "wx/textfile.h"
#include <GL/glew.h>


#define MIN_HEADER_SIZE 348
#define NII_HEADER_SIZE 352
#define HAVE_ZLIB


Anatomy::Anatomy(DatasetHelper* dh) {
	m_dh = dh;
	m_type = not_initialized;
	m_length = 0;
	m_bands = 0;
	m_frames = 0;
	m_rows = 0;
	m_columns = 0;
	m_repn = wxT("");
	m_xVoxel = 0.0;
	m_yVoxel = 0.0;
	m_zVoxel = 0.0;
	is_loaded = false;
	m_highest_value = 1.0;
	m_threshold = 0.00f;
	m_alpha = 1.0f;
	m_show = true;
	m_showFS = true;
	m_useTex = true;
	m_hasTreeId = false;
	m_GLuint = 0;
	m_roi = 0;
}

Anatomy::~Anatomy()
{
	delete[] m_floatDataset;
	m_dh->tensors_loaded = false;

	const GLuint* tex = &m_GLuint;
	glDeleteTextures(1, tex);
}

bool Anatomy::load(wxString filename)
{
	m_fullPath = filename;
#ifdef __WXMSW__
	m_name = filename.AfterLast('\\');
#else
	m_name = filename.AfterLast('/');
#endif
	// test for nifti
	if (m_name.AfterLast('.') == _T("nii"))
	{
		printf("detected nifti file\n");
		return loadNifti(filename);
	}
	else if (m_name.AfterLast('.') == _T("gz"))
	{
		printf("checking for compressed nifti file\n");
		wxString tmpName = m_name.BeforeLast('.');
		if (tmpName.AfterLast('.') == _T("nii"))
		{
			printf("found compressed nifti file\n");
			loadNifti(filename);
			return false;
		}
	}


	// read header file
	wxTextFile headerFile;
	bool flag = false;
	if (headerFile.Open(filename))
	{
		size_t i;
		wxString sLine;
		wxString sValue;
		wxString sLabel;
		long lTmpValue;
		for (i = 3 ; i < headerFile.GetLineCount() ; ++i)
		{
			sLine = headerFile.GetLine(i);
			sLabel = sLine.BeforeLast(' ');
			sValue = sLine.AfterLast(' ');
			sLabel.Trim(false);
			sLabel.Trim();
			if (sLabel.Contains(wxT("length:")))
			{
				flag = sValue.ToLong(&lTmpValue, 10);
				m_length = (int)lTmpValue;
			}
			if (sLabel == wxT("nbands:"))
			{
				flag = sValue.ToLong(&lTmpValue, 10);
				m_bands = (int)lTmpValue;
			}
			if (sLabel == wxT("nframes:"))
			{
				flag = sValue.ToLong(&lTmpValue, 10);
				m_frames = (int)lTmpValue;
			}
			if (sLabel == wxT("nrows:"))
			{
				flag = sValue.ToLong(&lTmpValue, 10);
				m_rows = (int)lTmpValue;
			}
			if (sLabel == wxT("ncolumns:"))
			{
				flag = sValue.ToLong(&lTmpValue, 10);
				m_columns = (int)lTmpValue;
			}
			if (sLabel == wxT("repn:"))
			//if (sLabel.Contains(wxT("repn:")))
			{
				m_repn = sValue;
			}
			if (sLabel.Contains(wxT("voxel:")))
			{
				wxString sNumber;
				sValue = sLine.AfterLast(':');
				sValue = sValue.BeforeLast('\"');
				sNumber = sValue.AfterLast(' ');
				flag = sNumber.ToDouble(&m_zVoxel);
				sValue = sValue.BeforeLast(' ');
				sNumber = sValue.AfterLast(' ');
				flag = sNumber.ToDouble(&m_yVoxel);
				sValue = sValue.BeforeLast(' ');
				sNumber = sValue.AfterLast('\"');
				flag = sNumber.ToDouble(&m_xVoxel);
			}
		}
	}
	headerFile.Close();

	if (m_repn.Cmp(wxT("ubyte")) == 0)
	{
		if (m_bands / m_frames == 1) {
			m_type = Head_byte;
		}
		else if (m_bands / m_frames == 3) {
			m_type = RGB;
		}
		else m_type = TERROR;
	}
	else if (m_repn.Cmp(wxT("short")) == 0) m_type = Head_short;
	else if (m_repn.Cmp(wxT("float")) == 0)
	{
		if (m_bands / m_frames == 3) {
			m_type = Vectors_;
		}
		else if (m_bands / m_frames == 6) {
			m_type = Tensors_;
		}
		else
			m_type = Overlay;
	}
	else m_type = TERROR;

	if (flag)
	{
		flag = false;
		wxFile dataFile;
		if (dataFile.Open(filename.BeforeLast('.')+ wxT(".ima")))
		{
			wxFileOffset nSize = dataFile.Length();
			if (nSize == wxInvalidOffset) return false;

			switch (m_type)
			{
			case Head_byte: {
				wxUint8* byteDataset = new wxUint8[nSize];
				if (dataFile.Read(byteDataset, (size_t) nSize) != nSize)
				{
					dataFile.Close();
					delete[] byteDataset;
					return false;
				}
				m_floatDataset = new float[nSize];
				for ( int i = 0 ; i < nSize ; ++i)
				{
					m_floatDataset[i] = (float)byteDataset[i] / 255.0;
				}
				delete[] byteDataset;
				flag = true;
			} break;

			case Head_short: {
				wxUint16* shortDataset = new wxUint16[nSize/2];
				if (dataFile.Read(shortDataset, (size_t) nSize) != nSize)
				{
					dataFile.Close();
					delete[] shortDataset;
					return false;
				}
				flag = true;

				float max = 65535.0;
				m_floatDataset = new float[nSize/2];
				for ( int i = 0 ; i < nSize/2 ; ++i)
				{
					m_floatDataset[i] = (float)shortDataset[i] / max;
				}
				delete[] shortDataset;
			} break;

			case Overlay: {
				float* floatDataset = new float[nSize/4];
				if (dataFile.Read(floatDataset, (size_t) nSize) != nSize)
				{
					dataFile.Close();
					delete[] floatDataset;
					return false;
				}
				m_floatDataset = new float[nSize/4];
				for ( int i = 0 ; i < nSize/4 ; ++i)
				{
					m_floatDataset[i] = (float)floatDataset[i];
				}
				delete[] floatDataset;
				flag = true;

			} break;

			case Vectors_: {
				m_floatDataset = new float[nSize/4];
				float* buffer = new float[nSize/4];
				if (dataFile.Read(buffer, (size_t) nSize) != nSize)
				{
					dataFile.Close();
					delete[] buffer;
					return false;
				}

				wxUint8 *pointbytes = (wxUint8*)buffer;
				wxUint8 temp;
				for ( int i = 0 ; i < nSize; i +=4)
				{
					temp  = pointbytes[i];
					pointbytes[i] = pointbytes[i+3];
					pointbytes[i+3] = temp;
					temp  = pointbytes[i+1];
					pointbytes[i+1] = pointbytes[i+2];
					pointbytes[i+2] = temp;
				}

				int offset = m_columns * m_rows;
				int startslize = 0;

				for (int i = 0 ; i < m_frames ; ++i)
				{
					startslize = i * offset * 3;
					for (int j = 0 ; j < offset ; ++j)
					{
						m_floatDataset[startslize + 3 * j] 		= buffer[startslize + j];
						m_floatDataset[startslize + 3 * j + 1] 	= buffer[startslize + offset + j];
						m_floatDataset[startslize + 3 * j + 2] 	= buffer[startslize + 2*offset + j];
					}
				}

				m_tensorField = new TensorField(m_dh, m_floatDataset, true);
				m_dh->tensors_loaded = true;

				flag = true;
				m_dh->vectors_loaded = true;
				m_dh->surface_isDirty = true;
			} break;

			case Tensors_: {
				m_floatDataset = new float[nSize/4];
				float* buffer = new float[nSize/4];
				if (dataFile.Read(buffer, (size_t) nSize) != nSize)
				{
					dataFile.Close();
					delete[] buffer;
					return false;
				}

				wxUint8 *pointbytes = (wxUint8*)buffer;
				wxUint8 temp;
				for ( int i = 0 ; i < nSize; i +=4)
				{
					temp  = pointbytes[i];
					pointbytes[i] = pointbytes[i+3];
					pointbytes[i+3] = temp;
					temp  = pointbytes[i+1];
					pointbytes[i+1] = pointbytes[i+2];
					pointbytes[i+2] = temp;
				}

				int offset = m_columns * m_rows;
				int startslize = 0;

				for (int i = 0 ; i < m_frames ; ++i)
				{
					startslize = i * offset * 6;
					for (int j = 0 ; j < offset ; ++j)
					{
						m_floatDataset[startslize + 3 * j] 		= buffer[startslize + j];
						m_floatDataset[startslize + 3 * j + 1] 	= buffer[startslize + offset + j];
						m_floatDataset[startslize + 3 * j + 2] 	= buffer[startslize + 2*offset + j];
						m_floatDataset[startslize + 3 * j + 3] 	= buffer[startslize + 3*offset + j];
						m_floatDataset[startslize + 3 * j + 4] 	= buffer[startslize + 4*offset + j];
						m_floatDataset[startslize + 3 * j + 5] 	= buffer[startslize + 5*offset + j];
					}
					delete[] buffer;
				}

				m_tensorField = new TensorField(m_dh, m_floatDataset, false);

				flag = true;
				m_dh->tensors_loaded = true;
			} break;

			case RGB: {
				wxUint8 *buffer = new wxUint8[nSize];
				m_floatDataset = new float[nSize];
				if (dataFile.Read(buffer, (size_t) nSize) != nSize)
				{
					dataFile.Close();
					delete[] buffer;
					return false;
				}
				flag = true;

				int offset = m_columns * m_rows;
				int startslize = 0;

				for (int i = 0 ; i < m_frames ; ++i)
				{
					startslize = i * offset * 3;
					for (int j = 0 ; j < offset ; ++j)
					{
						m_floatDataset[startslize + 3 * j    ] = (float)buffer[startslize + j           ] / 255.0;
						m_floatDataset[startslize + 3 * j + 1] = (float)buffer[startslize + offset + j  ] / 255.0;
						m_floatDataset[startslize + 3 * j + 2] = (float)buffer[startslize + 2*offset + j] / 255.0;
					}
				}
				delete[] buffer;
			} break;
			}
		}
		dataFile.Close();
	}

	is_loaded = flag;

	return flag;
}
// TODO
bool Anatomy::loadNifti(wxString filename)
{
	nifti_1_header hdr;

	FILE *fp;
	int ret;

	char* hdr_file;
	hdr_file = (char*) malloc(filename.length()+1);
	strcpy(hdr_file, (const char*) filename.mb_str(wxConvUTF8));



	/********** open and read header */
	fp = fopen(hdr_file,"r");
	if (fp == NULL) {
	        fprintf(stderr, "\nError opening header file %s\n",hdr_file);
	        exit(1);
	}
	ret = fread(&hdr, MIN_HEADER_SIZE, 1, fp);
	if (ret != 1) {
	        fprintf(stderr, "\nError reading header file %s\n",hdr_file);
	        exit(1);
	}
	fclose(fp);


	/********** print a little header information */
	fprintf(stderr, "\n%s header information:",hdr_file);
	fprintf(stderr, "\nXYZT dimensions: %d %d %d %d",hdr.dim[1],hdr.dim[2],hdr.dim[3],hdr.dim[4]);
	fprintf(stderr, "\nDatatype code and bits/pixel: %d %d",hdr.datatype,hdr.bitpix);
	fprintf(stderr, "\nScaling slope and intercept: %.6f %.6f",hdr.scl_slope,hdr.scl_inter);
	fprintf(stderr, "\nByte offset to data in datafile: %ld",(long)(hdr.vox_offset));
	fprintf(stderr, "\n");

	m_columns = hdr.dim[1]; // 160
	m_rows = hdr.dim[2]; // 200
	m_frames = hdr.dim[3]; // 160

	if (hdr.datatype == 2)
	{
		if (hdr.dim[4] == 1) {
			m_type = Head_byte;
		}
		else if (hdr.dim[4] == 3) {
			m_type = RGB;
		}
		else m_type = TERROR;
	}
	else if (hdr.datatype == 4) m_type = Head_short;

	else if (hdr.datatype == 16)
	{
		if (hdr.dim[4] == 3) {
			m_type = Vectors_;
		}
		/*
		else if (m_bands / m_frames == 6) {
			m_type = Tensors_;
		}
		*/
		else
			m_type = Overlay;
	}
	else m_type = TERROR;



	/********** open the datafile, jump to data offset */
	fp = fopen(hdr_file,"r");
	if (fp == NULL) {
	        fprintf(stderr, "\nError opening data file %s\n",hdr_file);
	        exit(1);
	}

	ret = fseek(fp, (long)(hdr.vox_offset), SEEK_SET);
	if (ret != 0) {
	        fprintf(stderr, "\nError doing fseek() to %ld in data file %s\n",(long)(hdr.vox_offset), hdr_file);
	        exit(1);
	}
	bool flag = false;

	switch (m_type)
	{
		case Head_byte: {
			/********** allocate buffer and read first 3D volume from data file */
			unsigned char *data=NULL;
			data = (unsigned char *) malloc(sizeof(unsigned char) * hdr.dim[1]*hdr.dim[2]*hdr.dim[3]);
			if (data == NULL) {
				fprintf(stderr, "\nError allocating data buffer for %s\n",hdr_file);
				return false;
			}
			ret = fread(data, sizeof(unsigned char), hdr.dim[1]*hdr.dim[2]*hdr.dim[3], fp);
			if (ret != hdr.dim[1]*hdr.dim[2]*hdr.dim[3]) {
				fprintf(stderr, "\nError reading volume 1 from %s (%d)\n",hdr_file,ret);
				return false;
			}

			int nSize = hdr.dim[1]*hdr.dim[2]*hdr.dim[3];
			m_floatDataset = new float[nSize];
			for ( int i = 0 ; i < nSize ; ++i)
			{
				m_floatDataset[i] = (float)data[nSize - i] / 255.0;
			}

			int fSize = m_frames * m_rows;
			float fBuffer;
			for (int i = 0 ; i < fSize ; ++i)
				for (int j = 0 ; j < m_columns /2 ; ++j)
				{
					fBuffer = m_floatDataset[j + i * m_columns];
					m_floatDataset[j + i * m_columns] = m_floatDataset[(m_columns - 1 - j) + i * m_columns];
					m_floatDataset[(m_columns - 1 - j) + i * m_columns] = fBuffer;
				}


			flag = true;
		} break;

		case Overlay: {
			m_floatDataset = new float[hdr.dim[1]*hdr.dim[2]*hdr.dim[3]];
			ret = fread(m_floatDataset, sizeof(float), hdr.dim[1]*hdr.dim[2]*hdr.dim[3], fp);
			if (ret != hdr.dim[1]*hdr.dim[2]*hdr.dim[3]) {
				fprintf(stderr, "\nError reading volume 1 from %s (%d)\n",hdr_file,ret);
				return false;

			}
			flag = true;
		} break;

		case RGB: {
			unsigned char *data=NULL;
			data = (unsigned char *) malloc(sizeof(unsigned char) * hdr.dim[1]*hdr.dim[2]*hdr.dim[3]*3);
			if (data == NULL) {
				fprintf(stderr, "\nError allocating data buffer for %s\n",hdr_file);
				return false;
			}
			ret = fread(data, sizeof(unsigned char), hdr.dim[1]*hdr.dim[2]*hdr.dim[3]*3, fp);
			if (ret != hdr.dim[1]*hdr.dim[2]*hdr.dim[3]*3) {
				fprintf(stderr, "\nError reading volume 1 from %s (%d)\n",hdr_file,ret);
				return false;
			}

			int nSize = hdr.dim[1]*hdr.dim[2]*hdr.dim[3];
			m_floatDataset = new float[nSize*3];

			for (int i = 0 ; i < nSize ; ++i)
			{

				m_floatDataset[i * 3    ] = (float)data[i] / 255.0;
				m_floatDataset[i * 3 + 1] = (float)data[nSize + i  ] / 255.0;
				m_floatDataset[i * 3 + 2] = (float)data[(2 * nSize) + i] / 255.0;

			}


			flag = true;
		} break;

		default:
			break;
	}

	fclose(fp);

	is_loaded = flag;

	return flag;

}

void Anatomy::generateTexture()
{
	glPixelStorei(GL_UNPACK_ALIGNMENT,1);
	glGenTextures(1, &m_GLuint);
	glBindTexture(GL_TEXTURE_3D, m_GLuint);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP);

	switch (m_type)
	{
	case Head_byte:
	case Head_short:
	case Overlay:
		glTexImage3D(GL_TEXTURE_3D,
			0,
			GL_RGBA,
			m_columns,
			m_rows,
			m_frames,
			0,
			GL_LUMINANCE,
			GL_FLOAT,
			m_floatDataset);
		break;
	case RGB:
		glTexImage3D(GL_TEXTURE_3D,
			0,
			GL_RGBA,
			m_columns,
			m_rows,
			m_frames,
			0,
			GL_RGB,
			GL_FLOAT,
			m_floatDataset);
		break;
	case Vectors_: {
		int size = m_rows*m_columns*m_frames*3;
		float *tempData = new float[size];
		for ( int i = 0 ; i < size ; ++i )
			tempData[i] = wxMax(m_floatDataset[i], -m_floatDataset[i]);


		glTexImage3D(GL_TEXTURE_3D,
			0,
			GL_RGBA,
			m_columns,
			m_rows,
			m_frames,
			0,
			GL_RGB,
			GL_FLOAT,
			tempData);
		delete[] tempData;
		break;
	}
	default:
		break;
	}
}

GLuint Anatomy::getGLuint()
{
	if (!m_GLuint)
		generateTexture();
	return m_GLuint;
}

float* Anatomy::getFloatDataset()
{
	return m_floatDataset;
}
