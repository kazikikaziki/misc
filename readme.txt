
����̃v���W�F�N�g��V�K�쐬����

A1. Visual Studio ���N�����A[�t�@�C��|�V�K�쐬|�v���W�F�N�g]���N���b�N
A2. �u�V�����v���W�F�N�g�v�_�C�A���O���o��̂ŁA���̃c���[����[�C���X�g�[���ς�/Visual C++/Windows�f�X�N�g�b�v]��I��
A3. �o�Ă����ꗗ����uWindows�f�X�N�g�b�v �E�B�U�[�h�v��I�����A�ۑ�������߂�[OK]�������i�����ł͉��� MyProject �� C:\MyProject �ɍ�������̂Ƃ���j
A4. �uWindows�f�X�N�g�b�v�v���W�F�N�g�v�_�C�A���O���o��̂ŁA�u�A�v���P�[�V�����̎�ށv�� �uWindows�A�v���P�[�V����(.exe)�v�ɂ��āA�u��̃v���W�F�N�g�v�����Ƀ`�F�b�N�����Ă����Ԃɂ���B���̃`�F�b�N�͂��ׂĊO���B���̂悤�ȏ�ԂɂȂ��Ă��邱�Ƃ��m�F����F

	�A�v���P�[�V�����̎��: �uWindows�A�v���P�[�V����(.exe)�v
	��̃v���W�F�N�g: �`�F�b�N����@�i�`�F�b�N���O���ƁA�]�v�� .cpp �t�@�C�������Ă���j
	�Z�L�����e�B�J�����C�t�T�C�N��: �`�F�b�N���� (�`�F�b�N������ƁA�񐄏��ȌÂ�������֐� strcpy �ɑ΂���x�����G���[�Ɋi�グ����A�R���p�C���ł��Ȃ��Ȃ�j
	ATL: �`�F�b�N����

A5. OK�{�^��������


���G���W���̃t�@�C�����v���W�F�N�g�ɒǉ�����

B1. �G�N�X�v���[���[���J���A��ō쐬�����v���W�F�N�g�̃t�H���_(C:\MyProject) ���J���B���ɂ� MyProject.sln �t�@�C���Ȃǂ������Ă���͂��B
B2. ������ Engine �t�H���_���ۂ��ƃR�s�[���ē���Ă���
B3. Visual Studio �ɖ߂�A�\�����[�V�����G�N�X�v���[���[��\��������
B4. �\�����[�V�����G�N�X�v���[������ "MyProject" ���ڂ��E�N���b�N���A�u�ǉ�|�V�����t�B���^�[�v��I�сA�K���Ȗ��O�i�����ł͉��� "mo" �Ƃ��Ă����j�Ō��肷��B
B5. ��Œǉ������t�B���^�[ "mo" �ɁA�����ق� C:\MyProejct �ɃR�s�[���� Engine �̒��ɂ��� core �t�H���_ (C:\MyProject\Engine\core) ���ۂ��ƃh���b�O�h���b�v���ăv���W�F�N�g�Ƀ\�[�X��ǉ�����
B6. �K�v�Ȃ�΁A C:\MyProject\Engine\libs �t�H���_�����l�ɒǉ����Ă���


�� �\�[�X�t�@�C����ǉ�����

C1. �\�����[�V�����G�N�X�v���[������ "MyProject" ���ڂ��E�N���b�N���A�u�ǉ�|�V�������ځv��I�сA�uC++�t�@�C��(.cpp)�v��I��Łu�ǉ��v���N���b�N����B�����ł͉��� "main.cpp" �Ƃ������O�ŕۑ��������̂Ƃ���
C2. �v�\�����[�V�����G�N�X�v���[������ "MyProject" ���ڂ��E�N���b�N���A�u�v���p�e�B�v��I�сA�v���p�e�B�E�B���h�E���J���B
C3.�u�\���v���p�e�B/�����J�[/�V�X�e���v��I�сA�T�u�V�X�e�����uWindows (/SUBSYSTEM:WINDOWS)�v�ɐݒ肷��B
  �����̍�Ƃ͏ȗ��\�B�ȗ������ꍇ�́uConsole (/SUBSYSTEM:CONSOLE)�v�ɂȂ��Ă���̂ŁA�ȉ��̃T���v���� WinMain(...) �ł͂Ȃ� int main() �Ə���

C4.�u�\���v���p�e�B/C/C++/�R�[�h�����v��I�сA�����^�C�����C�u�������u�}���`�X���b�h �f�o�b�O(/MTd)�v��I��
  �����̍�Ƃ͏ȗ��\�B�ȗ������ꍇ�́u�}���`�X���b�h �f�o�b�O DLL (/MDd)�v�ɂȂ��Ă���̂ŁA�쐬���� exe �𑼂̂o�b�Ŏ��s����Ƃ��ɒǉ���DLL��v�������\�������邭

C5.�u�\���v���p�e�B/C/C++/�v���v���Z�b�T�v��I�сA�v���Z�b�T�̒�`�� _CRT_SECURE_NO_WARNINGS ��ǉ�����
  �����̍�Ƃ͏ȗ��\�B�ȗ������ꍇ�̓R���p�C������ �uwarning C4996 ... �v�Ƃ����x������ʂɏo�邪�A�R���p�C�����̂͐�������


�� �R���p�C���ł��邱�Ƃ��m�F����

D1. main.cpp �Ɉȉ��̓��e�����̂܂܃R�s�y����

#include <Windows.h>
#include "engine/core/mo.h"
int WINAPI WinMain(HINSTANCE hCurrInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	return 0;
}

D2. �u�f�o�b�O|�f�o�b�O�̊J�n�v��I�Ԃ��AF5�������ăR���p�C�������s���A�G���[���N���Ȃ����Ƃ��m�F����B
�G���[���N�����Ɏ��s�ł�����OK �i�������Ȃ��v���O�����Ȃ̂Ŏ��s���Ă���u�ŏI���j
�������ŁA�u�������̃V���{�� _main ���֐� ....�v �Ƃ����G���[���o��ꍇ�A�T�u�V�X�e�����uWindows�v�ɂȂ��Ă��邱�Ƃ��m�F����i�菇 C3 ���Q�Ɓj
�������ŁA�uwarning C4996: 'strcpy': This function �Ȃ񂽂炩�񂽂� ...�v����ʂɏo��̂��C�ɂȂ�ꍇ�́A�v���v���Z�b�T�� _CRT_SECURE_NO_WARNINGS ���`���Ă��� �i�菇 C4 ���Q�Ɓj


